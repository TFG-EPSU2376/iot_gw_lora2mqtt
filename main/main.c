#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lora.h"
#include "mqtt_.h"

#define WIFI_MAXIMUM_RETRY 3
#define MQTT_BROKER_URI "mqtts://*.iot.eu-central-1.amazonaws.com"
#define MQTT_BROKER_PORT (8883)
#define MQTT_CLIENT_ID "GreengrassInternalDevice"

static const char *TAG = "MQTT_GATEWAY";

void extract_first_value(const char *message, char *value, size_t value_size)
{
	const char *start = strstr(message, "\"values\":[\"");
	if (start != NULL)
	{
		start += strlen("\"values\":[\"");
		const char *end = strchr(start, '"');
		if (end != NULL)
		{
			size_t len = end - start;
			if (len < value_size)
			{
				strncpy(value, start, len);
				value[len] = '\0';
			}
		}
	}
}

void sendLoraMessage(const char *message)
{

	uint8_t buf[256];
	int send_len = sprintf((char *)buf, "%s", message);
	lora_send_packet(buf, send_len);
	ESP_LOGI(pcTaskGetName(NULL), "%d byte packet sent...", 255);
	int lost = lora_packet_lost();
	if (lost != 0)
	{
		ESP_LOGW(pcTaskGetName(NULL), "%d packets lost", lost);
	}
	else
	{
		ESP_LOGI(pcTaskGetName(NULL), "No packet lost");
	}
}

void task_rx(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(NULL), "Starting RX task...");
	uint8_t buf[256];
	while (1)
	{
		lora_receive();
		if (lora_received())
		{
			int rxLen = lora_receive_packet(buf, sizeof(buf));
			ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received:[%.*s]", rxLen, rxLen, buf);

			buf[rxLen] = '\0';

			if (strstr((char *)buf, "\"type\":\"connected\"") != NULL)
			{
				ESP_LOGI(pcTaskGetName(NULL), "Mensaje de conexión detectado: %s", buf);

				char time_str[25];
				get_current_time_iso8601(time_str, sizeof(time_str));

				char first_value[64];
				extract_first_value((const char *)buf, first_value, sizeof(first_value));
				char response[256];
				snprintf(response, sizeof(response),
						 "{\"type\":\"accepted\",\"values\":[\"%s\",\"%s\"]}",
						 first_value, time_str);
				sendLoraMessage(response);
				sendLoraMessage(response);
				sendLoraMessage(response);
				sendLoraMessage(response);
				sendLoraMessage(response);
			}

			char mqtt_payload[512];
			snprintf(mqtt_payload, sizeof(mqtt_payload),
					 "{\"message\": %.*s, \"rssi\": %d, \"snr\": %.2f}",
					 rxLen, buf, lora_packet_rssi(), lora_packet_snr());

			send_mqtt_message("/EPSU2376/GW_Leader", mqtt_payload);
		}
		vTaskDelay(1);
	}
}

void app_main()
{
	ESP_LOGI(TAG, "Starting...");

	int cr = 1;
	int bw = 7;
	int sf = 7;
	init_lora(cr, bw, sf);
	xTaskCreate(&task_rx, "RX", 1024 * 3, NULL, 5, NULL);
	init_mqtt(MQTT_BROKER_URI, MQTT_BROKER_PORT, MQTT_CLIENT_ID);
}