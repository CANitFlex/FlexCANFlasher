/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "CanReceiver.h"

static const char *TAG = "example";

// WROOM - GPIO2 
// S3    - GPIO5 
// C3    - GPIO8 
#define BLINK_GPIO GPIO_NUM_2
#define BLINK_PERIOD 1000

static uint8_t s_led_state = 0;


static CanReceiver canReceiver;

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

static void led_task(void *arg)
{
    configure_led();

    while (1) {
        ESP_LOGI(TAG, "Turning the %s!", s_led_state == true ? "ON" : "OFF");
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}



extern "C" void app_main(void)
{ 

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mark app as valid: %s", esp_err_to_name(err));
    }

    CanReceiver::initTWAI();

        /* Configure the peripheral according to the LED type */

    xTaskCreate(led_task, "led_task", 8192, NULL, 1, NULL); // Low priority
    xTaskCreate(reinterpret_cast<TaskFunction_t>(&CanReceiver::canReceiverTask), "can_rx", 8192, NULL, 2, NULL); // Low priority
}
 