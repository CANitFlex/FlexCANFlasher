#include "freertos/FreeRTOS.h" // FreeRTOS.h muss zuerst eingebunden werden
#include "freertos/task.h"
#include "driver/uart.h"

#include "JSON_Parser_tests.h"
#include "ConfigBuilderTest.h"
#include "esp_system.h" // Für esp_restart()

extern "C" void app_main() {
    // UART-Treiber mit Standard-Konfiguration installieren
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    
    // Wir installieren den Treiber für UART 0 (der USB-Port)
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0);

    UNITY_BEGIN();
    JSON_Parser_tests::registerTests();
    ConfigBuilder_tests::registerTests();
    UNITY_END();

    // Jetzt funktioniert uart_wait_tx_done ohne Error!
    uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(500));
    
}