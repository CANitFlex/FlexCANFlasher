#ifndef MQTT_CONNECTION_BUILDER_H
#define MQTT_CONNECTION_BUILDER_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/queue.h"

#include "ConfigBuilder.h"
#include "cJSON.h"

// Message structure for OTA Task
typedef struct {
    char command[256];   // Firmware URL path
    char device_id[32];  // e.g. "controllerA"
    char version[16];    // e.g. "v1.2"
    uint32_t can_id;     // Target CAN Bus ID
} OTAMessage;

class MQTTConnectionBuilder {
    static constexpr const char *TAG = "MQTTConnectionBuilder";
    static QueueHandle_t ota_queue;
    static esp_mqtt_client_handle_t mqtt_client;

public:
    MQTTConnectionBuilder() {}

    // Create OTA Queue for inter-task communication
    static void createOTAQueue() {
        ota_queue = xQueueCreate(10, sizeof(OTAMessage));
        if (ota_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create OTA Queue");
        }
    }

    // Get OTA Queue Handle for other tasks
    static QueueHandle_t getOTAQueue() {
        return ota_queue;
    }

    // Event Handler for MQTT
    static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                                   int32_t event_id, void *event_data) {
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

        switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            
            // Get topic from ConfigBuilder
            const char* topic = ConfigBuilder::MQTT::getTopic();
            if (topic) {
                int msg_id = esp_mqtt_client_subscribe(event->client, topic, 0);
                ESP_LOGI(TAG, "Subscribed to topic: %s, msg_id=%d", topic, msg_id);
            } else {
                ESP_LOGE(TAG, "Failed to get MQTT topic from config");
            }
            break;
        }

        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

            // Parse JSON: {"path": "...", "can_id": 123, "device_id": "...", "version": "..."}
            char *data_copy = (char*)malloc(event->data_len + 1);
            if (!data_copy) {
                ESP_LOGE(TAG, "Failed to allocate memory for MQTT data");
                break;
            }
            memcpy(data_copy, event->data, event->data_len);
            data_copy[event->data_len] = '\0';

            cJSON *json = cJSON_Parse(data_copy);
            if (!json) {
                ESP_LOGE(TAG, "Failed to parse MQTT JSON data");
                free(data_copy);
                break;
            }

            OTAMessage msg = {};

            cJSON *path = cJSON_GetObjectItem(json, "path");
            if (cJSON_IsString(path)) {
                strncpy(msg.command, path->valuestring, sizeof(msg.command) - 1);
            }

            cJSON *can_id = cJSON_GetObjectItem(json, "can_id");
            if (cJSON_IsNumber(can_id)) {
                msg.can_id = (uint32_t)can_id->valuedouble;
            }

            cJSON *device_id = cJSON_GetObjectItem(json, "device_id");
            if (cJSON_IsString(device_id)) {
                strncpy(msg.device_id, device_id->valuestring, sizeof(msg.device_id) - 1);
            }

            cJSON *version = cJSON_GetObjectItem(json, "version");
            if (cJSON_IsString(version)) {
                strncpy(msg.version, version->valuestring, sizeof(msg.version) - 1);
            }

            if (xQueueSend(ota_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
                ESP_LOGI(TAG, "Message sent to OTA Queue: path=%s, can_id=0x%" PRIX32,
                         msg.command, msg.can_id);
            } else {
                ESP_LOGE(TAG, "Failed to send message to OTA Queue");
            }

            cJSON_Delete(json);
            free(data_copy);
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;

        default:
            ESP_LOGD(TAG, "MQTT Event ID: %" PRId32, event_id);
            break;
        }
    }

    // Start MQTT Connection
    static void startMQTT() {
        // Create Queue first
        createOTAQueue();

        // Get config from ConfigBuilder
        const char* broker = ConfigBuilder::MQTT::getBrokerURL();
        int port = ConfigBuilder::MQTT::getPort();
        
        if (!broker || port == 0) {
            ESP_LOGE(TAG, "Failed to get MQTT config");
            return;
        }

        // Build full broker URL
        static char full_url[256];
        snprintf(full_url, sizeof(full_url), "mqtt://%s:%d", broker, port);

        ESP_LOGI(TAG, "Connecting to MQTT Broker: %s", full_url);

        esp_mqtt_client_config_t mqtt_cfg = {};
        mqtt_cfg.broker.address.uri = full_url;

        mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(mqtt_client);
    }

    // Stop MQTT Connection
    static void stopMQTT() {
        if (mqtt_client) {
            esp_mqtt_client_stop(mqtt_client);
        }
    }

    static void mqtt_task_wrapper(void *arg) {
        ConfigBuilder config;
        MQTTConnectionBuilder::startMQTT();
        
        while (1) {
            vTaskDelay(10000 / portTICK_PERIOD_MS);  // Idle
        }
    }
};

// Initialize static members
QueueHandle_t MQTTConnectionBuilder::ota_queue = NULL;
esp_mqtt_client_handle_t MQTTConnectionBuilder::mqtt_client = NULL;

#endif // MQTT_CONNECTION_BUILDER_H