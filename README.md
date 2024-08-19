# LoRa to MQTT Gateway

This project is an implementation of a LoRa to MQTT gateway using the ESP32 platform. It receives messages over LoRa and forwards them to an MQTT broker. This can be used to bridge IoT devices communicating over LoRa with cloud or local servers via MQTT.

## Features

- Receive data from LoRa devices
- Forward data to an MQTT broker
- Includes packet loss detection and logging
- Parses and reformats received LoRa payloads before forwarding

## Hardware Requirements

- ESP32 Development Board
- LoRa Module (SX1276/SX1278)
- WiFi network for MQTT connectivity

## Software Requirements

- ESP-IDF Framework
- Configured LoRa and MQTT settings

## Installation

1. Clone the repository:

   ```sh
   git clone https://github.com/TFG-EPSU2376/iot_gw_lora2mqtt
   cd iot_gw_lora2mqtt
   ```

2. Set up the ESP-IDF environment:
   Follow the instructions on the [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/) to set up your ESP32 development environment.

3. Configure the project, especially the `sdkconfig` file if needed.

4. Build and flash the project:
   ```sh
   idf.py build
   idf.py flash
   idf.py monitor
   ```

## Configuration

Update the LoRa and MQTT configuration as needed in `main.c`:

```c
#define WIFI_MAXIMUM_RETRY 3
#define MQTT_BROKER_URI "mqtts://your_mqtt_broker_uri"
#define MQTT_BROKER_PORT (8883)
#define MQTT_CLIENT_ID "your_device_id"
```
