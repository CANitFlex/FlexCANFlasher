#ifndef CAN_FLASH_BUILDER_H
#define CAN_FLASH_BUILDER_H

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "MQTTConnectionBuilder.h"

// ─── Protocol Commands ────────────────────────────────────────────────────────
#define CAN_FLASH_CMD_BEGIN     0x01  // Gateway → Target: enter flash mode (4 bytes size LE)
#define CAN_FLASH_CMD_DATA      0x02  // Gateway → Target: [CMD][SEQ_LO][SEQ_HI][d0..d4] (5 bytes data)
#define CAN_FLASH_CMD_CRC       0x04  // Gateway → Target: [CMD][CRC_LO][CRC_HI] after each batch
#define CAN_FLASH_CMD_END       0x03  // Gateway → Target: all data sent
#define CAN_FLASH_CMD_ACK       0x10  // Target  → Gateway: acknowledged
#define CAN_FLASH_CMD_NACK      0x11  // Target  → Gateway: error

// ─── Config ───────────────────────────────────────────────────────────────────
#define CAN_FLASH_BATCH_SIZE    16    // ACK + CRC every N frames
#define CAN_DATA_BYTES          5     // Bytes per DATA frame (8 - CMD - SEQ_LO - SEQ_HI)
#define CAN_TX_RETRIES          3     // Retries on transmit failure
#define CAN_TX_GPIO             GPIO_NUM_19  // TODO: adjust to your hardware
#define CAN_RX_GPIO             GPIO_NUM_18  // TODO: adjust to your hardware

// ─── Message from OTA Task → CAN Flash Task ───────────────────────────────────
typedef struct {
    char     firmware_path[64];  // "/spiffs/controllerA_v1.2.bin"
    uint32_t can_id;             // Target CAN ID
    uint32_t firmware_size;      // Bytes
} CANFlashMessage;

class CANFlashBuilder {
    static constexpr const char *TAG = "CANFlashBuilder";
    static QueueHandle_t can_flash_queue;
    static bool twai_initialized;

public:

    // ─── Queue ────────────────────────────────────────────────────────────────
    static void createCANFlashQueue() {
        can_flash_queue = xQueueCreate(5, sizeof(CANFlashMessage));
        if (!can_flash_queue)
            ESP_LOGE(TAG, "Failed to create CAN Flash Queue");
    }

    static QueueHandle_t getCANFlashQueue() { return can_flash_queue; }

    // ─── TWAI Init ────────────────────────────────────────────────────────────
    static esp_err_t initTWAI() {
        if (twai_initialized) return ESP_OK;

        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
        twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "TWAI install failed: %s", esp_err_to_name(err));
            return err;
        }
        err = twai_start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "TWAI start failed: %s", esp_err_to_name(err));
            twai_driver_uninstall();
            return err;
        }
        twai_initialized = true;
        ESP_LOGI(TAG, "TWAI ready (TX=GPIO%d RX=GPIO%d 500kbit/s)", CAN_TX_GPIO, CAN_RX_GPIO);
        return ESP_OK;
    }

    // ─── CRC16 (CCITT) ────────────────────────────────────────────────────────
    static uint16_t crc16(const uint8_t *data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= (uint16_t)data[i] << 8;
            for (int b = 0; b < 8; b++)
                crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
        return crc;
    }

    // ─── Transmit with Retry ──────────────────────────────────────────────────
    static esp_err_t sendWithRetry(twai_message_t *msg, int retries = CAN_TX_RETRIES) {
        for (int i = 0; i < retries; i++) {
            esp_err_t err = twai_transmit(msg, pdMS_TO_TICKS(1000));
            if (err == ESP_OK) return ESP_OK;
            ESP_LOGW(TAG, "Transmit failed (%d/%d): %s", i + 1, retries, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGE(TAG, "Transmit failed after %d retries", retries);
        return ESP_FAIL;
    }

    // ─── Wait for ACK/NACK ────────────────────────────────────────────────────
    static bool waitForACK(uint32_t response_can_id, uint32_t timeout_ms) {
        twai_message_t rx_msg;
        TickType_t start   = xTaskGetTickCount();
        TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

        while ((xTaskGetTickCount() - start) < timeout) {
            if (twai_receive(&rx_msg, pdMS_TO_TICKS(100)) == ESP_OK) {
                if (rx_msg.identifier == response_can_id && rx_msg.data_length_code >= 1) {
                    if (rx_msg.data[0] == CAN_FLASH_CMD_ACK)  return true;
                    if (rx_msg.data[0] == CAN_FLASH_CMD_NACK) {
                        ESP_LOGE(TAG, "NACK from 0x%" PRIX32, response_can_id);
                        return false;
                    }
                }
            }
        }
        ESP_LOGE(TAG, "ACK timeout from 0x%" PRIX32, response_can_id);
        return false;
    }

    // ─── FLASH_BEGIN ──────────────────────────────────────────────────────────
    static esp_err_t sendFlashBegin(uint32_t target_id, uint32_t firmware_size) {
        twai_message_t msg = {};
        msg.identifier       = target_id;
        msg.data_length_code = 5;
        msg.data[0] = CAN_FLASH_CMD_BEGIN;
        msg.data[1] = (firmware_size >>  0) & 0xFF;
        msg.data[2] = (firmware_size >>  8) & 0xFF;
        msg.data[3] = (firmware_size >> 16) & 0xFF;
        msg.data[4] = (firmware_size >> 24) & 0xFF;
        return sendWithRetry(&msg);
    }

    // ─── FLASH_DATA (with sequence number) ───────────────────────────────────
    // Frame layout: [CMD=0x02][SEQ_LO][SEQ_HI][d0][d1][d2][d3][d4]  → 8 bytes max
    static esp_err_t sendFlashData(uint32_t target_id, uint16_t seq,
                                   const uint8_t *data, int len) {
        twai_message_t msg = {};
        msg.identifier       = target_id;
        msg.data_length_code = 3 + len;   // CMD + SEQ_LO + SEQ_HI + data
        msg.data[0] = CAN_FLASH_CMD_DATA;
        msg.data[1] = (seq >> 0) & 0xFF;
        msg.data[2] = (seq >> 8) & 0xFF;
        memcpy(&msg.data[3], data, len);
        return sendWithRetry(&msg);
    }

    // ─── FLASH_CRC (after each batch) ─────────────────────────────────────────
    static esp_err_t sendFlashCRC(uint32_t target_id, uint16_t crc) {
        twai_message_t msg = {};
        msg.identifier       = target_id;
        msg.data_length_code = 3;
        msg.data[0] = CAN_FLASH_CMD_CRC;
        msg.data[1] = (crc >> 0) & 0xFF;
        msg.data[2] = (crc >> 8) & 0xFF;
        return sendWithRetry(&msg);
    }

    // ─── FLASH_END ────────────────────────────────────────────────────────────
    static esp_err_t sendFlashEnd(uint32_t target_id) {
        twai_message_t msg = {};
        msg.identifier       = target_id;
        msg.data_length_code = 1;
        msg.data[0]          = CAN_FLASH_CMD_END;
        return sendWithRetry(&msg);
    }

    // ─── Main Flash Function ──────────────────────────────────────────────────
    static esp_err_t flashFirmwareOverCAN(const CANFlashMessage *flash_msg) {
        uint32_t target_id   = flash_msg->can_id;
        uint32_t response_id = target_id + 1;

        ESP_LOGI(TAG, "CAN flash → 0x%" PRIX32 " | %s | %" PRIu32 " bytes",
                 target_id, flash_msg->firmware_path, flash_msg->firmware_size);

        // SPIFFS: ESP_ERR_INVALID_STATE = already mounted, that's fine
        esp_vfs_spiffs_conf_t spiffs_conf = {
            .base_path          = "/spiffs",
            .partition_label    = NULL,
            .max_files          = 10,
            .format_if_mount_failed = false,
        };
        esp_err_t err = esp_vfs_spiffs_register(&spiffs_conf);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
            return ESP_FAIL;
        }

        FILE *file = fopen(flash_msg->firmware_path, "rb");
        if (!file) {
            ESP_LOGE(TAG, "Cannot open: %s", flash_msg->firmware_path);
            esp_vfs_spiffs_unregister(NULL);
            return ESP_FAIL;
        }

        // ── Step 1: BEGIN ──────────────────────────────────────────────────
        err = sendFlashBegin(target_id, flash_msg->firmware_size);
        if (err != ESP_OK) { fclose(file); esp_vfs_spiffs_unregister(NULL); return err; }

        if (!waitForACK(response_id, 5000)) {
            ESP_LOGE(TAG, "No ACK for FLASH_BEGIN");
            fclose(file); esp_vfs_spiffs_unregister(NULL); return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Target 0x%" PRIX32 " in flash mode", target_id);

        // ── Step 2: DATA ───────────────────────────────────────────────────
        uint8_t  buffer[CAN_DATA_BYTES];
        int      read_len;
        uint32_t total_sent  = 0;
        uint16_t frame_count = 0;

        // CRC accumulator for current batch
        uint8_t  batch_buf[CAN_FLASH_BATCH_SIZE * CAN_DATA_BYTES];
        uint32_t batch_buf_len = 0;

        while ((read_len = fread(buffer, 1, sizeof(buffer), file)) > 0) {

            err = sendFlashData(target_id, frame_count, buffer, read_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Data frame %" PRIu16 " failed", frame_count);
                fclose(file); esp_vfs_spiffs_unregister(NULL); return err;
            }

            // Give the receiver time to drain its TWAI RX queue (default depth: 5)
            vTaskDelay(pdMS_TO_TICKS(2));

            // Accumulate for CRC
            memcpy(batch_buf + batch_buf_len, buffer, read_len);
            batch_buf_len += read_len;

            total_sent += read_len;
            frame_count++;

            // Every BATCH_SIZE frames: send CRC then wait for ACK
            if (frame_count % CAN_FLASH_BATCH_SIZE == 0) {
                uint16_t batch_crc = crc16(batch_buf, batch_buf_len);

                err = sendFlashCRC(target_id, batch_crc);
                if (err != ESP_OK) {
                    fclose(file); esp_vfs_spiffs_unregister(NULL); return err;
                }

                if (!waitForACK(response_id, 3000)) {
                    ESP_LOGE(TAG, "NACK/timeout after frame %" PRIu16, frame_count);
                    fclose(file); esp_vfs_spiffs_unregister(NULL); return ESP_FAIL;
                }

                ESP_LOGD(TAG, "Batch OK | %" PRIu32 " / %" PRIu32 " bytes | CRC=0x%04X",
                         total_sent, flash_msg->firmware_size, batch_crc);

                batch_buf_len = 0;  // Reset batch buffer
            }
        }

        // Final incomplete batch
        if (frame_count % CAN_FLASH_BATCH_SIZE != 0 && batch_buf_len > 0) {
            uint16_t batch_crc = crc16(batch_buf, batch_buf_len);

            err = sendFlashCRC(target_id, batch_crc);
            if (err != ESP_OK) {
                fclose(file); esp_vfs_spiffs_unregister(NULL); return err;
            }

            if (!waitForACK(response_id, 3000)) {
                ESP_LOGE(TAG, "NACK/timeout on final batch");
                fclose(file); esp_vfs_spiffs_unregister(NULL); return ESP_FAIL;
            }
        }

        ESP_LOGI(TAG, "Data sent: %" PRIu32 " bytes | %" PRIu16 " frames", total_sent, frame_count);

        // ── Step 3: END ────────────────────────────────────────────────────
        err = sendFlashEnd(target_id);
        if (err != ESP_OK) { fclose(file); esp_vfs_spiffs_unregister(NULL); return err; }

        if (!waitForACK(response_id, 10000)) {
            ESP_LOGE(TAG, "No final ACK from target");
            fclose(file); esp_vfs_spiffs_unregister(NULL); return ESP_FAIL;
        }

        ESP_LOGI(TAG, "✓ CAN flash complete → 0x%" PRIX32, target_id);
        fclose(file);
        esp_vfs_spiffs_unregister(NULL);
        return ESP_OK;
    }

    // ─── FreeRTOS Task ────────────────────────────────────────────────────────
    static void canFlashTask(void *arg) {
        if (initTWAI() != ESP_OK) {
            ESP_LOGE(TAG, "TWAI init failed – task exit");
            vTaskDelete(NULL);
            return;
        }

        CANFlashMessage msg;
        while (1) {
            if (xQueueReceive(can_flash_queue, &msg, portMAX_DELAY)) {
                esp_err_t ret = flashFirmwareOverCAN(&msg);
                ESP_LOGI(TAG, "Flash 0x%" PRIX32 " %s",
                         msg.can_id, ret == ESP_OK ? "OK" : "FAILED");
            }
        }
    }
};

QueueHandle_t CANFlashBuilder::can_flash_queue = NULL;
bool          CANFlashBuilder::twai_initialized = false;

#endif // CAN_FLASH_BUILDER_H