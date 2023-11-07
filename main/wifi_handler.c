/* WiFi station with reconnection
 *
 * File: wifi_handler.c
 * Author: antusystem <aleantunes95@gmail.com>
 * Date: 26th October 2023
 * Description: Handler Wifi init, config and (re)connection.
 * 
 * This file is subject to the terms of the Apache License, Version 2.0, January 2004.
 */

// #include <stdio.h>
#include <string.h>
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

static const char *WIFI_TAG = "wifi station rc";
static char *WIFI_LED_TAG = "WIFI LED";


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t led_event;


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries
*/
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define LED_BIT            BIT2


static int s_retry_num = 0;
static uint8_t connection_status = 0;

/**
  * @brief  WiFi Event Handler
  * 
  * This function handles the WiFi Events.
  * @param arg Aditional argument to pass
  * @param event_base esp_event_base_t variable
  * @param event_id int32_t variable id
  * @param event_data event data
  */
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(WIFI_TAG, "WIFI_EVENT_STA_START Event");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(WIFI_TAG, "Device disconnected from WiFi");
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else {
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGW(WIFI_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(WIFI_TAG, "IP_EVENT_STA_GOT_IP Event");
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
  * @brief  WiFi Station Mode Initialization
  * 
  * This function initialize WiFi in Station Mode.
  */
void wifi_init_sta(void){
    // ESP_LOGI(WIFI_TAG, "Creating events group");
    s_wifi_event_group = xEventGroupCreate();

    // ESP_LOGI(WIFI_TAG, "Init Netif");
    ESP_ERROR_CHECK(esp_netif_init());

    // ESP_LOGI(WIFI_TAG, "Creating Event Loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // ESP_LOGI(WIFI_TAG, "Netif WiFi Station");
    esp_netif_create_default_wifi_sta();

    // ESP_LOGI(WIFI_TAG, "Basic WiFi configuration");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    // ESP_LOGI(WIFI_TAG, "Register WiFi instance");
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL,
            &instance_any_id
        )
    );
    // ESP_LOGI(WIFI_TAG, "Register IP instance");
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &event_handler,
            NULL,
            &instance_got_ip
        )
    );

    // ESP_LOGI(WIFI_TAG, "Configurating WiFi");
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASSWORD,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .scan_method = ESP_WIFI_SCAN_METHOD,
            .sort_method = ESP_WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi = ESP_WIFI_SCAN_RSSI_THRESHOLD,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = ESP_WIFI_H2E_IDENTIFIER,

        },
    };
    // ESP_LOGI(WIFI_TAG, "Checking WiFi mode");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    // ESP_LOGI(WIFI_TAG, "Checking WiFi configuration");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    // ESP_LOGI(WIFI_TAG, "Starting WiFi");
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
}

/**
  * @brief  WiFi Task
  * This task initialize the WiFi and try to reconnect in case that connection is lost.
  * 
  * @attention 1. Take in consideration the ways to reconnect indicated in the docs(https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/api-guides/wifi.html), where it is stated that: the reconnection may not connect the same AP if there are more than one APs with the same SSID. The reconnection always select current best APs to connect.
  */
void ESP_WIFI_Task(void *P){
    ESP_LOGW(WIFI_TAG,"Init Wifi Task");
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    EventBits_t bits;
    ESP_LOGI(WIFI_TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    for(;;){
        ESP_LOGW(WIFI_TAG, "*********** WiFi waiting bits ***********");
        // Wating until WIFI_CONNECTED_BIT or WIFI_FAIL_BIT is set
        bits = xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(WIFI_TAG, "WIFI_CONNECTED_BIT bit");
            ESP_LOGI(
                WIFI_TAG,
                "connected to ap SSID:%s password:%s",
                ESP_WIFI_SSID,
                ESP_WIFI_PASSWORD
            );
            // Clearing bits
            ESP_LOGI(WIFI_TAG, "Limpiando los bits");
            xEventGroupClearBits(
                s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | LED_BIT
            );
            // Changing conecction status
            connection_status = 1;
            // xEventGroupClearBits(
            //     s_wifi_event_group,
            //     LED_BIT
            // );
            ESP_LOGW(WIFI_TAG, "---------- Connection established ----------");
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(WIFI_TAG, "WIFI_FAIL_BIT bit");
            // Set the LED bit so it starts to blink
            xEventGroupSetBits(
                s_wifi_event_group,
                LED_BIT
            );
            // Changing conecction status
            connection_status = 0;
            ESP_LOGI(
                WIFI_TAG,
                "Failed to connect to SSID:%s, password:%s",
                ESP_WIFI_SSID,
                ESP_WIFI_PASSWORD
            );
            // Waiting time before next retry, set to 10s
            vTaskDelay(RETRY_TIME / portTICK_PERIOD_MS);
            // Retry to connect to WiFi
            esp_wifi_connect();
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");

        } else {
            ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
        }
    }
}

/**
  * @brief  Configure WiFi Led
  * 
  * This function does the setup for the led that indicates the status of the WiFi connection.
  * If the led is blinking then there is not a Wifi connection, if it is still, then there is WiFi connection.
  */
static void configure_led(void){
    ESP_LOGI(WIFI_LED_TAG, "LED_GPIO: %d", LED_GPIO);
    // Reset GPIO Pin
    gpio_reset_pin(LED_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

/**
  * @brief  LED Task
  * 
  * This task setup the led and turn it on according to the WiFi connection status.
  */
void LED_Blink(void *P){
    ESP_LOGW(WIFI_LED_TAG,"Init LED_Blink");
    uint8_t led_state = 1;
    configure_led();
    EventBits_t led_bits;
    led_event = xEventGroupCreate();
    for(;;){
        // This change in the led state can also be done using
        // WIFI_CONNECTED_BIT and WIFI_FAIL_BIT, but this is simple
        // though it uses a global variable (connection_status)
        if (connection_status == 0) {
            // Blinks until it is connected again
            gpio_set_level(LED_GPIO, led_state);
            led_state = !led_state;
            vTaskDelay(500 / portTICK_PERIOD_MS);
        } else {
            // Turning LED On
            gpio_set_level(LED_GPIO, 1);
            ESP_LOGW(WIFI_LED_TAG, "*********** LED Waiting bits ***********");
            // Waiting until LED_BIT is set
            led_bits = xEventGroupWaitBits(
                s_wifi_event_group,
                LED_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY
            );
            ESP_LOGW(WIFI_LED_TAG, "*********** LED Finish bits ***********");
        }
    }
}
