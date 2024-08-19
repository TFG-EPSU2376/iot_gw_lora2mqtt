
#include "esp_wifi.h"

void start_wifi_ap();
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

void wifi_init_sta(char *ssid, char *password, char *mqtt_broker, int mqtt_port, char *client_id);