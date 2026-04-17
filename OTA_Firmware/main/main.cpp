#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"

#include <stdio.h>
#include <sys/stat.h>

#include "nvs.h"
#include "nvs_flash.h"
#include <sys/socket.h>

#include "WifiConnectionBuilder.h"
#include "OTAConnectionBuilder.h"
#include "MQTTConnectionBuilder.h"
#include "CANFlashBuilder.h"



#define HASH_LEN 32

static const char *TAG = "OTA_Firmware_FLasher";

#define OTA_URL_SIZE 256

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}


extern "C" void app_main(void)
{

    ESP_LOGI(TAG, "OTA example app_main start");
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    get_sha256_of_partitions();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    static ConfigBuilder configBuilder;
    static OTAConnectionBuilder otaConnectionBuilder;

    
    static WifiConnectionBuilder wifiConnectionBuilder;
    static MQTTConnectionBuilder mqttConnectionBuilder;
    

    wifiConnectionBuilder.initialize();

    MQTTConnectionBuilder::startMQTT();

    CANFlashBuilder::createCANFlashQueue();

    xTaskCreate(reinterpret_cast<TaskFunction_t>(&OTAConnectionBuilder::createOTATask), "ota_task", 8192, NULL, 5, NULL);
    xTaskCreate(reinterpret_cast<TaskFunction_t>(&CANFlashBuilder::canFlashTask), "can_flash_task", 8192, NULL, 5, NULL);
}
