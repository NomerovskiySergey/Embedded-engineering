idf_component_register(
  SRCS
    "main.cpp"
    "domain/limits_model.cpp"
    "domain/poll_schedule.cpp"
    "providers/limits_parser.cpp"
    "platform/limits_client.cpp"
    "platform/waveshare_display_profile.cpp"
    "platform/wifi_manager.cpp"
    "ui/dashboard.cpp"
  INCLUDE_DIRS "."
  PRIV_REQUIRES
    driver
    esp_event
    esp_http_client
    esp_lcd
    esp_netif
    esp_wifi
    lvgl
    nvs_flash
)

idf_component_get_property(lvgl_component lvgl__lvgl COMPONENT_LIB)
target_compile_definitions(${lvgl_component} PRIVATE
  LV_DRAW_SW_SUPPORT_RGB565_SWAPPED=1
)
