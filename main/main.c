/* WiFi station with reconnection
 *
 * File: main.c
 * Author: antusystem <aleantunes95@gmail.com>
 * Date: 26th October 2023
 * Description: Example of how to implement a Wifi Reconnection.
 * 
 * This file is subject to the terms of the Apache License, Version 2.0, January 2004.
 */

// #include <string.h>
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_wifi_config.h"

static const char *TAG = "wifi station rc";

/**
  * @brief  Log Chip Information
  *
  * This function logs the chip information.
  */
void chip_information(void){
    char *CHIP_TAG = "Chip information";
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    ESP_LOGI(
        CHIP_TAG,
        "This is %s chip with %d CPU core(s), WiFi%s%s, ",
        CONFIG_IDF_TARGET,
        chip_info.cores,
        (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
        (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : ""
    );

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    ESP_LOGI(CHIP_TAG, "silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }
    ESP_LOGI(
        CHIP_TAG,
         "%" PRIu32 "MB %s flash", flash_size / (uint32_t)(1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external"
    );
    ESP_LOGI(
        CHIP_TAG,
        "Minimum free heap size: %" PRIu32 " bytes",
        esp_get_minimum_free_heap_size()
    );
}

void app_main(void)
{
    ESP_LOGW(TAG, "****** Starting ********");
    chip_information();

    xTaskCreatePinnedToCore(&LED_Blink, "LED_Blink", 1024*2, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(&ESP_WIFI_Task, "My_task_wifi", 1024*4, NULL, 5, NULL, 1);

    // ESP_LOGI(TAG, "ESP_WIFI_SSID: %s", ESP_WIFI_SSID);
    // ESP_LOGI(TAG, "ESP_WIFI_PASSWORD: %s", ESP_WIFI_PASSWORD);
    // ESP_LOGI(TAG, "ESP_MAXIMUM_RETRY: %d", ESP_MAXIMUM_RETRY);
    // ESP_LOGI(TAG, "ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD: %d", ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD);
    // ESP_LOGI(TAG, "LED_GPIO: %d", LED_GPIO);
    ESP_LOGW(TAG, "****** Finish Main ********");

}
