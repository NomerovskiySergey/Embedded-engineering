#include <stdbool.h>
#include <stdio.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "led_strip.h"

// Replace these with the credentials for your 2.4 GHz Wi-Fi network.
#define WIFI_SSID "Kyivstar_37"
#define WIFI_PASSWORD "nomer356131"

// Onboard RGB LED: GPIO8, RGB order for this board revision. Brightness: 32/255.
#define RGB_LED_GPIO 8
#define LED_BRIGHTNESS 32

#define WIFI_CONNECTED_BIT BIT0

static const char *const kTag = "led_web_server";
static EventGroupHandle_t s_wifi_event_group;
static led_strip_handle_t s_rgb_led;
static bool s_red_led_on;
static bool s_blue_led_on;

static void apply_led_states(void)
{
  ESP_ERROR_CHECK(led_strip_set_pixel(s_rgb_led, 0,
                                     s_red_led_on ? LED_BRIGHTNESS : 0,
                                     0,
                                     s_blue_led_on ? LED_BRIGHTNESS : 0));
  ESP_ERROR_CHECK(led_strip_refresh(s_rgb_led));
}

static esp_err_t send_redirect_to_root(httpd_req_t *request)
{
  httpd_resp_set_status(request, "303 See Other");
  httpd_resp_set_hdr(request, "Location", "/");
  return httpd_resp_send(request, NULL, 0);
}

static esp_err_t root_handler(httpd_req_t *request)
{
  static const char kPage[] =
      "<!doctype html><html><head><meta name=\"viewport\" "
      "content=\"width=device-width,initial-scale=1\"><style>"
      "body{font-family:system-ui;margin:2rem;max-width:34rem}"
      "section{border:1px solid #ddd;border-radius:12px;padding:1rem;margin:.8rem 0}"
      "a{display:inline-block;padding:.65rem 1rem;margin-right:.5rem;border-radius:7px;"
      "background:#16803c;color:#fff;text-decoration:none}.stop{background:#a32d2d}"
      ".on{color:#16803c}.off{color:#a32d2d}</style></head><body>"
      "<h1>ESP32 onboard RGB</h1><p>Red + blue = purple. Both OFF = LED off.</p>"
      "<section><h2>Red channel <span id=\"red\">...</span></h2>"
      "<a href=\"/red/on\">Turn on</a><a class=\"stop\" href=\"/red/off\">Turn off</a></section>"
      "<section><h2>Blue channel <span id=\"blue\">...</span></h2>"
      "<a href=\"/blue/on\">Turn on</a><a class=\"stop\" href=\"/blue/off\">Turn off</a></section>"
      "<script>fetch('/state').then(r=>r.json()).then(s=>{for(const n of ['red','blue'])"
      "{const e=document.getElementById(n);e.textContent=s[n]?'ON':'OFF';"
      "e.className=s[n]?'on':'off'}})</script></body></html>";

  httpd_resp_set_type(request, "text/html");
  return httpd_resp_sendstr(request, kPage);
}

static esp_err_t state_handler(httpd_req_t *request)
{
  char response[48];
  const int length = snprintf(response, sizeof(response),
                              "{\"red\":%s,\"blue\":%s}",
                              s_red_led_on ? "true" : "false",
                              s_blue_led_on ? "true" : "false");
  if (length < 0 || length >= (int)sizeof(response))
  {
    return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "Unable to create state response");
  }
  httpd_resp_set_type(request, "application/json");
  return httpd_resp_send(request, response, length);
}

static esp_err_t red_on_handler(httpd_req_t *request)
{
  s_red_led_on = true;
  apply_led_states();
  return send_redirect_to_root(request);
}

static esp_err_t red_off_handler(httpd_req_t *request)
{
  s_red_led_on = false;
  apply_led_states();
  return send_redirect_to_root(request);
}

static esp_err_t blue_on_handler(httpd_req_t *request)
{
  s_blue_led_on = true;
  apply_led_states();
  return send_redirect_to_root(request);
}

static esp_err_t blue_off_handler(httpd_req_t *request)
{
  s_blue_led_on = false;
  apply_led_states();
  return send_redirect_to_root(request);
}

static void start_web_server(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(httpd_start(&server, &config));

  const httpd_uri_t routes[] = {
      {.uri = "/", .method = HTTP_GET, .handler = root_handler},
      {.uri = "/state", .method = HTTP_GET, .handler = state_handler},
      {.uri = "/red/on", .method = HTTP_GET, .handler = red_on_handler},
      {.uri = "/red/off", .method = HTTP_GET, .handler = red_off_handler},
      {.uri = "/blue/on", .method = HTTP_GET, .handler = blue_on_handler},
      {.uri = "/blue/off", .method = HTTP_GET, .handler = blue_off_handler},
  };

  for (size_t index = 0; index < sizeof(routes) / sizeof(routes[0]); ++index)
  {
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &routes[index]));
  }
}

static void wifi_event_handler(void *argument, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  (void)argument;
  (void)event_data;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    ESP_ERROR_CHECK(esp_wifi_connect());
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    ESP_LOGW(kTag, "Wi-Fi disconnected; reconnecting");
    ESP_ERROR_CHECK(esp_wifi_connect());
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static void initialise_wifi(void)
{
  s_wifi_event_group = xEventGroupCreate();
  configASSERT(s_wifi_event_group != NULL);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init_config));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  wifi_config_t wifi_config = {0};
  snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
  snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", WIFI_PASSWORD);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                      portMAX_DELAY);
}

void app_main(void)
{
  ESP_ERROR_CHECK(nvs_flash_init());

  const led_strip_config_t strip_config = {
      .strip_gpio_num = RGB_LED_GPIO,
      .max_leds = 1,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
  };
  const led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10000000,
      .mem_block_symbols = 64,
      .flags.with_dma = false, // ESP32-C6 RMT has no DMA support.
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_rgb_led));
  apply_led_states();

  initialise_wifi();
  start_web_server();
  ESP_LOGI(kTag, "Web server started. Find the ESP32 IP address in the monitor log.");
}
