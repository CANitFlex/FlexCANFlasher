#ifndef OTA_CONNECTION_BUILDER_H
#define OTA_CONNECTION_BUILDER_H
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include <functional>
#include <cstdint>
#include <fstream>

#include "mbedtls/sha256.h"
#include "ConfigBuilder.h"
#include "MQTTConnectionBuilder.h"
#include "CANFlashBuilder.h"

extern const uint8_t _binary_server_cert_pem_start[];
extern const uint8_t _binary_server_cert_pem_end[];
extern const uint8_t _binary_server_key_pem_start[];
extern const uint8_t _binary_server_key_pem_end[];



class OTAConnectionBuilder {
public:
    OTAConnectionBuilder() {}

    static constexpr char *TAG = "OTAConnectionBuilder OTA_Firmware";

    static void createOTATask() {
    QueueHandle_t ota_queue = MQTTConnectionBuilder::getOTAQueue();
    QueueHandle_t can_queue = CANFlashBuilder::getCANFlashQueue();
    OTAMessage msg;

    while (1) {
        if (xQueueReceive(ota_queue, &msg, portMAX_DELAY)) {
            ESP_LOGI(TAG, "OTA request: device=%s ver=%s can_id=0x%" PRIX32,
                     msg.device_id, msg.version, msg.can_id);

            // Dateipfad bauen: /spiffs/controllerA_v1.2.bin
            char filepath[64];
            snprintf(filepath, sizeof(filepath), "/spiffs/firmware.bin");

            static esp_http_client_config_t httpConfig = {};
            httpConfig = buildHttpConfig(msg.command);
            esp_https_ota_config_t ota_config = { .http_config = &httpConfig };

            // Download → SPIFFS
            uint32_t firmware_size = 0;
            esp_err_t ret = downloadFirmware(&ota_config, &firmware_size);

            if (httpConfig.url) free((void*)httpConfig.url);

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Download failed for %s", filepath);
                continue;
            }

            if (firmware_size == 0) {
                ESP_LOGE(TAG, "Firmware size is 0, aborting");
                continue;
            }
            ESP_LOGI(TAG, "Firmware size: %" PRIu32 " bytes", firmware_size);

            // ─── Übergabe an CAN Flash Task ──────────────────────────────
            CANFlashMessage can_msg = {};
            strncpy(can_msg.firmware_path, filepath, sizeof(can_msg.firmware_path) - 1);
            can_msg.can_id        = 0x100;  // TODO: use msg.can_id from MQTT message
            can_msg.firmware_size = firmware_size;

            if (xQueueSend(can_queue, &can_msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "→ CAN queue: %s to 0x%" PRIX32, filepath, msg.can_id);
            } else {
                ESP_LOGE(TAG, "CAN queue full!");
            }
        }
    }
}

    static esp_err_t downloadFirmware(const esp_https_ota_config_t* ota_config, uint32_t *out_firmware_size = nullptr) {
        ESP_LOGI(TAG, "Downloading firmware...");

        // SPIFFS mounten
        esp_vfs_spiffs_conf_t spiffs_conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true,
        };
        esp_err_t spiffs_err = esp_vfs_spiffs_register(&spiffs_conf);
        if (spiffs_err != ESP_OK && spiffs_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(spiffs_err));
            return ESP_FAIL;
        }

        FILE* file = fopen("/spiffs/firmware.bin", "wb");
        if (!file) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            esp_vfs_spiffs_unregister(NULL);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "File opened successfully for writing");

        esp_http_client_handle_t client = esp_http_client_init(ota_config->http_config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to init HTTP client");
            fclose(file);
            esp_vfs_spiffs_unregister(NULL);
            return ESP_FAIL;
        }

        // ✅ Verbindung öffnen + Header senden
        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            fclose(file);
            esp_http_client_cleanup(client);
            esp_vfs_spiffs_unregister(NULL);
            return err;
        }

        // ✅ Response-Header lesen → gibt Content-Length zurück
        int content_length = esp_http_client_fetch_headers(client);
        ESP_LOGI(TAG, "Content-Length: %d bytes", content_length);

        int status_code = esp_http_client_get_status_code(client);
        if (status_code != 200) {
            ESP_LOGE(TAG, "HTTP status: %d", status_code);
            fclose(file);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            esp_vfs_spiffs_unregister(NULL);
            return ESP_FAIL;
        }

        // ✅ SHA256 Init
        mbedtls_sha256_context sha_ctx;
        mbedtls_sha256_init(&sha_ctx);
        mbedtls_sha256_starts(&sha_ctx, 0);

        char buffer[1024];
        int read_len;
        int total_written = 0;

        // ✅ Jetzt streamen – funktioniert nach open() + fetch_headers()
        while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
            size_t written = fwrite(buffer, 1, read_len, file);
            if (written != (size_t)read_len) {
                ESP_LOGE(TAG, "fwrite mismatch: wrote %d of %d bytes", written, read_len);
                err = ESP_FAIL;
                break;
            }
            mbedtls_sha256_update(&sha_ctx, (unsigned char*)buffer, read_len);
            total_written += read_len;
            ESP_LOGD(TAG, "Progress: %d / %d bytes", total_written, content_length);
        }

        if (read_len < 0) {
            ESP_LOGE(TAG, "Error reading HTTP stream: %d", read_len);
            err = ESP_FAIL;
        }

        // SHA256 finalisieren
        unsigned char sha256_raw[32];
        mbedtls_sha256_finish(&sha_ctx, sha256_raw);
        mbedtls_sha256_free(&sha_ctx);

        char sha256_hex[65];
        for (int i = 0; i < 32; i++) {
            snprintf(sha256_hex + i * 2, 3, "%02x", sha256_raw[i]);
        }
        ESP_LOGI(TAG, "Firmware written: %d bytes", total_written);
        ESP_LOGI(TAG, "Firmware SHA256:  %s", sha256_hex);

        // Size check
        if (err == ESP_OK && content_length > 0 && total_written != content_length) {
            ESP_LOGE(TAG, "Size mismatch: expected %d, got %d", content_length, total_written);
            err = ESP_ERR_INVALID_SIZE;
        } else if (err == ESP_OK) {
            ESP_LOGI(TAG, "Download complete: %d bytes", total_written);
        }

        if (out_firmware_size) {
            *out_firmware_size = (uint32_t)total_written;
        }

        fclose(file);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        esp_vfs_spiffs_unregister(NULL);
        return err;
    }

    static esp_http_client_config_t buildHttpConfig(const char* firmware_url) {
        if (!firmware_url || strlen(firmware_url) == 0) {
            ESP_LOGE(TAG, "Invalid firmware URL");
            return {};
        }
        const char* prefix = "https://";
        const char* ip = ConfigBuilder::ServerConfig::getExternalIP();
        int port = ConfigBuilder::ServerConfig::getPort();
        if (!ip || port <= 0) {
            ESP_LOGE(TAG, "Failed to get server IP or port from config");
            return {};
        }
        int url_len = strlen(prefix) + strlen(ip) + 1 + 5 + strlen(firmware_url) + 1;
        char* full_url = (char*)malloc(url_len);
        if (!full_url) {
            ESP_LOGE(TAG, "Failed to allocate memory for full URL");
            return {};
        }
        snprintf(full_url, url_len, "%s%s:%d/%s", prefix, ip, port, firmware_url);
        ESP_LOGI(TAG, "Built full URL: %s", full_url);

        esp_http_client_config_t config = {
            .url = full_url,
            .cert_pem = (char*)_binary_server_cert_pem_start,
            .event_handler = _http_event_handler,
            .keep_alive_enable = true,
        };

        ESP_LOGI(TAG, "HTTP Config built with URL: %s", full_url);
        return config;
    }

    static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
        switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
        }
        return ESP_OK;
    }
};

#endif