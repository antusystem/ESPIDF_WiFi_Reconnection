# WiFi station with reconnection

This example shows how to use the Wi-Fi Station functionality of the Wi-Fi driver of ESP for connecting to an Access Point and also implementing a reconnetion and adding a LED to show the connection status.


**Note:** Take in consideration the ways to reconnect indicated in the [docs](https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/api-guides/wifi.html), where it is stated that: **"_the reconnection may not connect the same AP if there are more than one APs with the same SSID. The reconnection always select current best APs to connect_"**.

## How to use example

### Configure the project

Open the project configuration menu (`idf.py menuconfig`).

In the `WiFi station with reconnection` menu:

* Set the Wi-Fi configuration.
    * Set `WiFi SSID`.
    * Set `WiFi Password`.
* Set LED GPIO:
    * Indicate the GPIO that is connected to the LED. Default 2.

Optional: If you need, change the other options according to your requirements.

## Hardware Required

You need a ESP32 basic Devboard, this was tested with a NodeMCU.

### Pin Assignment:

To use the LED to show the connection status you must indicate the GPIO, in the case of the Devboard used in this example the GPIO is 2.

| ESP32  | DESCRIPTION |
| ------ | ----------- |
| GPIO2  | LED         |

## Log

* Last compile: 07 November 2023.
* Last test: 07 November 2023.
* Last compile espidf version: `v 5.1.1`.
* VSCode ESP-IDF Visual Studio Code Extension `v 1.6.5`.


## License

Apache License, Version 2.0, January 2004.

## Version

`v 1.2.1`
