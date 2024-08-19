/* The example of ESP-IDF
 *
 * This sample code is in the public domain.
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "esp_system.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"

#include "mqtt_client.h"
#include "wifi.h"
#include "mqtt_.h"

static const char *TAG = "MQTT_GATEWAY";
static int s_retry_num = 0;
static esp_mqtt_client_handle_t mqtt_client = NULL;

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_root_cert_auth_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_root_cert_auth_crt_end");

void get_current_time_iso8601(char *buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRId32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        char time_str[25]; // Buffer para almacenar la fecha y hora formateada
        get_current_time_iso8601(time_str, sizeof(time_str));
        char message[128];
        snprintf(message, sizeof(message), "{\"type\":\"connected\",\"values\":[\"GW_Lora2MQTT\",\"%s\"]}", time_str);
        msg_id = esp_mqtt_client_publish(client, "/topic/connect", message, 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, "/topic/GW_Lora2MQTT", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(char *mqtt_broker, int mqtt_port, char *client_id)
{
    ESP_LOGI(TAG, "Iniciando cliente MQTT ...");
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = (const char *)mqtt_broker,
            .address.port = mqtt_port,
            .verification.certificate = (const char *)server_cert_pem_start,
        },
        .credentials = {
            .authentication = {
                .certificate = (const char *)client_cert_pem_start,
                .key = (const char *)client_key_pem_start,
            },
            .client_id = (const char *)client_id,
        },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

void init_mqtt(char *mqtt_broker, int mqtt_port, char *client_id)
{
    char *ssid = NULL;
    char *password = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta(ssid, password, mqtt_broker, mqtt_port, client_id);
    ESP_LOGI(TAG, "Iniciando cliente MQTT ...");
    mqtt_app_start(mqtt_broker, mqtt_port, client_id);
}

void send_mqtt_message(const char *topic, const char *message)
{
    if (mqtt_client == NULL)
    {
        ESP_LOGE(TAG, "MQTT client is not initialized");
        return;
    }
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, message, 0, 1, 0);
    ESP_LOGI(TAG, "Sent publish successful, msg_id=%d", msg_id);
}
