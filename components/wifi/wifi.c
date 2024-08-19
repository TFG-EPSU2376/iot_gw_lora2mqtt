#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "mqtt_.h"
#include <stdio.h>
#include <time.h>
#include "esp_sntp.h"
#include "lwip/dns.h"

#define WIFI_MAXIMUM_RETRY 3

#define WIFI_SSID "default_ssid"
#define WIFI_PASS "default_password"
#define NVS_NAMESPACE "storage"
#define NVS_KEY_SSID "wifi_ssid"
#define NVS_KEY_PASS "wifi_pass"

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

static const char *TAG = "MQTT_GATEWAY_WIFI";
static int s_retry_num = 0;
char *mqtt_broker_ = NULL;
int mqtt_port_ = 0;
char *client_id_ = NULL;

void obtain_time(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
    {
        ESP_LOGI("SNTP", "Esperando que la hora se sincronice... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry == retry_count)
    {
        ESP_LOGI("SNTP", "No se pudo obtener la hora.");
    }
    else
    {
        ESP_LOGI("SNTP", "Hora sincronizada: %s", asctime(&timeinfo));
    }
}

void start_wifi_sta(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = ""}};

    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando a WiFi... SSID: %s", ssid);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < WIFI_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            ESP_LOGI(TAG, "connect to the AP fail");

            nvs_handle_t nvs_handle;
            ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));

            ESP_ERROR_CHECK(nvs_erase_key(nvs_handle, NVS_KEY_SSID));
            ESP_ERROR_CHECK(nvs_erase_key(nvs_handle, NVS_KEY_PASS));

            ESP_ERROR_CHECK(nvs_commit(nvs_handle));
            nvs_close(nvs_handle);

            ESP_LOGI(TAG, "Credenciales eliminadas de NVS, reiniciando...");
            esp_restart();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        obtain_time();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        mqtt_app_start(mqtt_broker_, mqtt_port_, client_id_);
    }
}

static esp_err_t wifi_setup_get_handler(httpd_req_t *req)
{
    const char *html_form = "<!DOCTYPE html><html><body><form action=\"/wifi-setup\" method=\"post\">"
                            "SSID:<br><input type=\"text\" name=\"ssid\"><br>"
                            "Password:<br><input type=\"text\" name=\"password\"><br><br>"
                            "<input type=\"submit\" value=\"Submit\"></form></body></html>";
    httpd_resp_send(req, html_form, strlen(html_form));
    return ESP_OK;
}

static const httpd_uri_t wifi_setup_page = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = wifi_setup_get_handler,
    .user_ctx = NULL};

static esp_err_t wifi_credentials_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        buf[ret] = '\0';
    }

    char *ssid = strtok(buf, "&");
    char *password = strtok(NULL, "&");

    // Guardar en NVS
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid + 5));
    ESP_ERROR_CHECK(nvs_set_str(nvs_handle, NVS_KEY_PASS, password + 9));
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    nvs_close(nvs_handle);

    const char *resp = "Credenciales guardadas. Reiniciando...";
    httpd_resp_send(req, resp, strlen(resp));

    esp_restart();

    return ESP_OK;
}

static const httpd_uri_t wifi_credentials = {
    .uri = "/wifi-setup",
    .method = HTTP_POST,
    .handler = wifi_credentials_post_handler,
    .user_ctx = NULL};

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &wifi_setup_page);
        httpd_register_uri_handler(server, &wifi_credentials);
    }
}

void start_wifi_ap()
{
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "GW_Lora_MQTT",
            .ssid_len = strlen("GW_Lora_MQTT"),
            .channel = 0,
            .password = "EPSU2376!",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK}};
    if (strlen("EPSU2376!") == 0)
    {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Punto de acceso iniciado. SSID: %s", ap_config.ap.ssid);
    start_webserver();
}

void wifi_init_sta(char *WifiSsid, char *WifiPassword, char *mqtt_broker, int mqtt_port, char *client_id)
{

    mqtt_broker_ = mqtt_broker;
    mqtt_port_ = mqtt_port;
    client_id_ = client_id;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));
    size_t ssid_len = 32;
    size_t pass_len = 64;
    char ssid[ssid_len];
    char password[pass_len];

    ret = nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &ssid_len);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "No se encontró el SSID en NVS");
        start_wifi_ap();
    }
    else
    {
        ret = nvs_get_str(nvs_handle, NVS_KEY_PASS, password, &pass_len);
        if (ret != ESP_OK)
        {
            ESP_LOGI(TAG, "No se encontró la contraseña en NVS");
            start_wifi_ap();
        }
        else
        {
            ESP_LOGI(TAG, "Conectando a WiFi guardada. SSID: %s", ssid);
            start_wifi_sta(ssid, password);
            xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        }
    }

    nvs_close(nvs_handle);
}