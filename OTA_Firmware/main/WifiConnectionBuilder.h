#ifndef WIFI_CONNECTION_BUILDER_H
#define WIFI_CONNECTION_BUILDER_H
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "ConfigBuilder.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_interface.h"
#include "esp_netif.h"
#include "esp_eth.h"

static int s_retry_num = 0;
static const int EXAMPLE_ESP_MAXIMUM_RETRY = 5;
static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI("WifiConnectionBuilder", "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI("WifiConnectionBuilder","connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WifiConnectionBuilder", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

class WifiConnectionBuilder {
    const char *TAG = "WifiConnectionBuilder OTA_Firmware";

public:
    WifiConnectionBuilder() {}

    void initialize(){
        s_wifi_event_group = xEventGroupCreate();
        
        // Initialize WiFi with default configuration
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        // Register event handlers for WiFi and IP events
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            NULL));

        // Configure WiFi with SSID and password
        wifi_config_t wifi_config = {};
        strncpy((char *)wifi_config.sta.ssid, ConfigBuilder::RouterConfig::getSSID(), sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, ConfigBuilder::RouterConfig::getPassword(), sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "Connecting to Wi-Fi SSID:%s ...", ConfigBuilder::RouterConfig::getSSID());
        
        // Wait for either connection success or max retries reached
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "connected to ap SSID:%s", ConfigBuilder::RouterConfig::getSSID());
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s", ConfigBuilder::RouterConfig::getSSID());
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
    }

};
#endif