idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES led_strip esp_wifi esp_event esp_netif esp_http_server nvs_flash)
