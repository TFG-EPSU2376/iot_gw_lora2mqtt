set(component_srcs)

idf_component_register(SRCS "wifi.c" "${component_srcs}"
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi nvs_flash esp_http_server mqtt_)
