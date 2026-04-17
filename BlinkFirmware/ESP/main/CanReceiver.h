/*
 * CAN Receiver – ESP32 Firmware
 *
 * Listens on a configured CAN ID. When it receives a FLASH_BEGIN frame
 * it enters flash mode, accepts DATA frames (with sequence numbers),
 * verifies CRC16 per batch, and writes the firmware to the OTA partition.
 * After FLASH_END it validates, sets the new partition as boot, and restarts.
 *
 * Protocol  (must match CANFlashBuilder.h on the gateway side):
 *   CMD 0x01  FLASH_BEGIN  [CMD][SIZE_0][SIZE_1][SIZE_2][SIZE_3]
 *   CMD 0x02  FLASH_DATA   [CMD][SEQ_LO][SEQ_HI][d0..d4]   (up to 5 data bytes)
 *   CMD 0x04  FLASH_CRC    [CMD][CRC_LO][CRC_HI]            (CRC16-CCITT of batch)
 *   CMD 0x03  FLASH_END    [CMD]
 *
 * Response CAN ID = MY_CAN_ID + 1
 *   CMD 0x10  ACK
 *   CMD 0x11  NACK
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "driver/twai.h"

// ─── Configuration ────────────────────────────────────────────────────────────
#define MY_CAN_ID              0x100
#define RESPONSE_CAN_ID        (MY_CAN_ID + 1)

#define CAN_TX_GPIO             GPIO_NUM_18
#define CAN_RX_GPIO             GPIO_NUM_19

#define CAN_FLASH_CMD_BEGIN    0x01
#define CAN_FLASH_CMD_DATA     0x02
#define CAN_FLASH_CMD_END      0x03
#define CAN_FLASH_CMD_CRC      0x04
#define CAN_FLASH_CMD_ACK      0x10
#define CAN_FLASH_CMD_NACK     0x11

#define CAN_FLASH_BATCH_SIZE   16
#define CAN_DATA_BYTES         5

class CanReceiver {
    static const char *TAG;
public:
    CanReceiver() {
        canQueue = xQueueCreate(10, sizeof(twai_message_t));
    }

    static SemaphoreHandle_t canSemaphore;

    static void initTWAI() {
        twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
            CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
        g_config.rx_queue_len = 32;
        twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "TWAI install failed: %s", esp_err_to_name(err));
            return;
        }
        err = twai_start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "TWAI start failed: %s", esp_err_to_name(err));
            twai_driver_uninstall();
            return;
        }
        ESP_LOGI(TAG, "TWAI ready (TX=GPIO%d RX=GPIO%d 500kbit/s) – listening on CAN ID 0x%X",
                 CAN_TX_GPIO, CAN_RX_GPIO, MY_CAN_ID);

        // Create the semaphore
        canSemaphore = xSemaphoreCreateBinary();
        if (canSemaphore == NULL) {
            ESP_LOGE(TAG, "Failed to create CAN semaphore");
        }
    }

        static void canReceiverTask(void *arg) {
            ESP_LOGI(TAG, "CAN Receiver Task started.");
            twai_message_t rx;

            while (1) {
                // Diese Zeile ersetzt Semaphore UND ISR. 
                // Der Task schläft hier mit 0% CPU, bis die HARDWARE meldet: "Daten da!"
                if (twai_receive(&rx, portMAX_DELAY) == ESP_OK) {
                    
                    // --- JETZT "ÜBERBIETEN" WIR DAS BLINKEN ---
        
                    ESP_LOGI(TAG, "CAN Message ID: 0x%X", (unsigned int)rx.identifier);
                    
                    if (rx.identifier == MY_CAN_ID && rx.data_length_code >= 1) {
                        uint8_t cmd = rx.data[0];

                        // 2. Flash-Kommando Logik
                        if (cmd == CAN_FLASH_CMD_BEGIN && rx.data_length_code >= 5) {
                            uint32_t firmware_size =
                                (uint32_t)rx.data[1]        |
                                ((uint32_t)rx.data[2] << 8) |
                                ((uint32_t)rx.data[3] << 16)|
                                ((uint32_t)rx.data[4] << 24);

                            ESP_LOGI(TAG, "FLASH_BEGIN - Size: %" PRIu32 " bytes", firmware_size);
                            
                            // Hier springst du in die Flash-Routine
                            handleFlashSession(firmware_size);
                        }
                    }
                }
            }
        }

private:
    static uint16_t crc16(const uint8_t *data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= (uint16_t)data[i] << 8;
            for (int b = 0; b < 8; b++)
                crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
        return crc;
    }

    static esp_err_t sendResponse(uint8_t cmd) {
        twai_message_t msg = {};
        msg.identifier       = RESPONSE_CAN_ID;
        msg.data_length_code = 1;
        msg.data[0]          = cmd;
        return twai_transmit(&msg, pdMS_TO_TICKS(1000));
    }

    static esp_err_t sendACK() {
        return sendResponse(CAN_FLASH_CMD_ACK);
    }

    static esp_err_t sendNACK() {
        return sendResponse(CAN_FLASH_CMD_NACK);
    }

    static bool receiveFrame(twai_message_t *out, uint32_t timeout_ms) {
        TickType_t start   = xTaskGetTickCount();
        TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

        while ((xTaskGetTickCount() - start) < timeout) {
            if (twai_receive(out, pdMS_TO_TICKS(100)) == ESP_OK) {
                if (out->identifier == MY_CAN_ID) {
                    return true;
                }
            }
        }
        return false;
    }

    static void handleFlashSession(uint32_t firmware_size) {
        ESP_LOGI(TAG, "Entering flash mode – expecting %" PRIu32 " bytes", firmware_size);

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
        if (!update_partition) {
            ESP_LOGE(TAG, "No OTA partition available (forced selection failed)");
            sendNACK();
            return;
        }
        ESP_LOGI(TAG, "Writing to partition: %s (offset 0x%" PRIx32 ")",
                 update_partition->label, update_partition->address);

        esp_ota_handle_t ota_handle;
        esp_err_t err = esp_ota_begin(update_partition, firmware_size, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
            sendNACK();
            return;
        }

        sendACK();
        ESP_LOGI(TAG, "ACK sent for FLASH_BEGIN – ready to receive data");

        uint32_t total_received  = 0;
        uint16_t expected_seq    = 0;
        uint16_t batch_frame_cnt = 0;

        uint8_t  batch_buf[CAN_FLASH_BATCH_SIZE * CAN_DATA_BYTES];
        uint32_t batch_buf_len = 0;

        bool flash_ok = false;

        while (true) {
            twai_message_t rx;
            if (!receiveFrame(&rx, 10000)) {
                ESP_LOGE(TAG, "Timeout waiting for data (received %" PRIu32 "/%" PRIu32 ")",
                         total_received, firmware_size);
                sendNACK();
                esp_ota_abort(ota_handle);
                return;
            }

            if (rx.data_length_code < 1) continue;
            uint8_t cmd = rx.data[0];

            if (cmd == CAN_FLASH_CMD_DATA) {
                if (rx.data_length_code < 4) {
                    ESP_LOGE(TAG, "DATA frame too short (%d bytes)", rx.data_length_code);
                    sendNACK();
                    esp_ota_abort(ota_handle);
                    return;
                }

                uint16_t seq = rx.data[1] | ((uint16_t)rx.data[2] << 8);
                if (seq != expected_seq) {
                    ESP_LOGE(TAG, "Sequence mismatch: expected %" PRIu16 ", got %" PRIu16,
                             expected_seq, seq);
                    sendNACK();
                    esp_ota_abort(ota_handle);
                    return;
                }

                int data_len = rx.data_length_code - 3;
                const uint8_t *data = &rx.data[3];

                err = esp_ota_write(ota_handle, data, data_len);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
                    sendNACK();
                    esp_ota_abort(ota_handle);
                    return;
                }

                memcpy(batch_buf + batch_buf_len, data, data_len);
                batch_buf_len += data_len;

                total_received += data_len;
                expected_seq++;
                batch_frame_cnt++;

                ESP_LOGD(TAG, "DATA seq=%" PRIu16 " len=%d total=%" PRIu32,
                         seq, data_len, total_received);
            } else if (cmd == CAN_FLASH_CMD_CRC) {
                if (rx.data_length_code < 3) {
                    ESP_LOGE(TAG, "CRC frame too short");
                    sendNACK();
                    esp_ota_abort(ota_handle);
                    return;
                }

                uint16_t received_crc = rx.data[1] | ((uint16_t)rx.data[2] << 8);
                uint16_t computed_crc = crc16(batch_buf, batch_buf_len);

                if (received_crc != computed_crc) {
                    ESP_LOGE(TAG, "CRC mismatch: received 0x%04X, computed 0x%04X",
                             received_crc, computed_crc);
                    sendNACK();
                    esp_ota_abort(ota_handle);
                    return;
                }

                ESP_LOGD(TAG, "Batch CRC OK (0x%04X) – %" PRIu32 "/%" PRIu32 " bytes",
                         computed_crc, total_received, firmware_size);

                batch_buf_len   = 0;
                batch_frame_cnt = 0;
                sendACK();
            } else if (cmd == CAN_FLASH_CMD_END) {
                ESP_LOGI(TAG, "FLASH_END received – total %" PRIu32 " bytes", total_received);

                if (total_received != firmware_size) {
                    ESP_LOGE(TAG, "Size mismatch: expected %" PRIu32 ", got %" PRIu32,
                             firmware_size, total_received);
                    sendNACK();
                    esp_ota_abort(ota_handle);
                    return;
                }

                flash_ok = true;
                break;
            } else {
                ESP_LOGW(TAG, "Unexpected command 0x%02X during flash", cmd);
            }
        }

        if (!flash_ok) {
            ESP_LOGE(TAG, "Flash session incomplete");
            sendNACK();
            esp_ota_abort(ota_handle);
            return;
        }

        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
            sendNACK();
            return;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
            sendNACK();
            return;
        }

        ESP_LOGI(TAG, "OTA partition set – sending final ACK and restarting...");
        sendACK();

        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }

    static QueueHandle_t canQueue;
};

// Define static members
QueueHandle_t CanReceiver::canQueue = xQueueCreate(10, sizeof(twai_message_t));
SemaphoreHandle_t CanReceiver::canSemaphore = NULL;
const char *CanReceiver::TAG = "Can Receiver";