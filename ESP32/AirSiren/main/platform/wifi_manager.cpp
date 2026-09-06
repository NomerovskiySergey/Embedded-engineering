#include "platform/wifi_manager.h"

#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define AIRSIREN_WIFI_SSID "YOUR_2_4_GHZ_WIFI_NAME"
#define AIRSIREN_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

namespace alertsiren {

void wifiEventHandler(void *context, const esp_event_base_t base,
                      const int32_t eventId, void *) {
  auto *const manager = static_cast<WifiManager *>(context);
  if (base == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
    manager->_connected.store(false, std::memory_order_release);
    esp_wifi_connect();
  } else if (base == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
    manager->_connected.store(true, std::memory_order_release);
  }
}

bool WifiManager::credentialsConfigured() const {
  return std::strcmp(AIRSIREN_WIFI_SSID, "YOUR_2_4_GHZ_WIFI_NAME") != 0 &&
         AIRSIREN_WIFI_SSID[0] != '\0';
}

esp_err_t WifiManager::begin() {
  esp_err_t result = esp_netif_init();
  if (result != ESP_OK) return result;
  result = esp_event_loop_create_default();
  if (result != ESP_OK) return result;
  if (esp_netif_create_default_wifi_sta() == nullptr) return ESP_FAIL;

  const wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
  result = esp_wifi_init(&initConfig);
  if (result != ESP_OK) return result;
  result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &wifiEventHandler, this);
  if (result != ESP_OK) return result;
  result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      &wifiEventHandler, this);
  if (result != ESP_OK) return result;

  wifi_config_t wifiConfig = {};
  std::snprintf(reinterpret_cast<char *>(wifiConfig.sta.ssid),
                sizeof(wifiConfig.sta.ssid), "%s", AIRSIREN_WIFI_SSID);
  std::snprintf(reinterpret_cast<char *>(wifiConfig.sta.password),
                sizeof(wifiConfig.sta.password), "%s", AIRSIREN_WIFI_PASSWORD);
  wifiConfig.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  result = esp_wifi_set_mode(WIFI_MODE_STA);
  if (result != ESP_OK) return result;
  result = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
  if (result != ESP_OK) return result;

  const esp_sntp_config_t sntpConfig =
      ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  result = esp_netif_sntp_init(&sntpConfig);
  if (result != ESP_OK) return result;
  return esp_wifi_start();
}

bool WifiManager::isConnected() const {
  return _connected.load(std::memory_order_acquire);
}

}  // namespace alertsiren
