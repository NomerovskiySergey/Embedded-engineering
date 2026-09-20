#include <cstdint>
#include <cstdlib>
#include <ctime>

#include "domain/limits_model.h"
#include "domain/poll_schedule.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "platform/limits_client.h"
#include "platform/wifi_manager.h"
#include "ui/dashboard.h"

namespace {

constexpr char kTag[] = "codex_limits";
constexpr uint32_t kPollIntervalMs = 60000U;
constexpr uint32_t kFirstRetryMs = 5000U;
constexpr uint32_t kMaximumRetryMs = 60000U;
constexpr uint32_t kFreshnessMs = 300000U;
constexpr uint32_t kRedrawIntervalMs = 10000U;
constexpr time_t kMinimumValidEpoch = 1704067200;

uint32_t nowMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

}  // namespace

extern "C" void app_main() {
  esp_err_t nvsResult = nvs_flash_init();
  if (nvsResult == ESP_ERR_NVS_NO_FREE_PAGES ||
      nvsResult == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    nvsResult = nvs_flash_init();
  }
  ESP_ERROR_CHECK(nvsResult);
  const bool timezoneReady =
      setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1) == 0;
  if (timezoneReady) tzset();

  static codexlimits::Dashboard dashboard;
  static codexlimits::WifiManager wifi;
  static codexlimits::LimitsClient client;
  codexlimits::PollSchedule schedule(kPollIntervalMs, kFirstRetryMs,
                                     kMaximumRetryMs);
  codexlimits::SnapshotStore store;

  ESP_ERROR_CHECK(dashboard.begin(nowMs()));
  ESP_ERROR_CHECK(wifi.begin());
  ESP_ERROR_CHECK(client.begin());
  if (!wifi.credentialsConfigured() || !client.configured()) {
    ESP_LOGE(kTag, "Copy main/secrets.example.h to main/secrets.h and configure it");
  }

  bool fetchInFlight = false;
  bool previousWifi = false;
  uint32_t lastRedrawMs = 0U;
  bool redraw = true;

  while (true) {
    const uint32_t currentMs = nowMs();
    dashboard.service(currentMs);
    const bool connected = wifi.isConnected();
    if (connected != previousWifi) {
      previousWifi = connected;
      redraw = true;
    }

    if (connected && !fetchInFlight && schedule.isDue(currentMs)) {
      if (client.requestFetch()) fetchInFlight = true;
    }

    codexlimits::FetchResult result = {};
    if (client.takeResult(result)) {
      fetchInFlight = false;
      if (result.valid) {
        store.recordSuccess(result.snapshot, currentMs);
        schedule.recordSuccess(currentMs);
        ESP_LOGI(kTag, "Codex limits updated");
      } else {
        store.recordFailure();
        schedule.recordFailure(currentMs);
        ESP_LOGW(kTag, "Codex limits bridge request failed");
      }
      redraw = true;
    }

    if (static_cast<uint32_t>(currentMs - lastRedrawMs) >=
        kRedrawIntervalMs) {
      redraw = true;
    }
    if (redraw) {
      const bool fresh = codexlimits::displayDataFresh(
          store.hasData(), connected, store.lastFetchFailed(),
          store.isFresh(currentMs, kFreshnessMs));
      const time_t currentEpoch = std::time(nullptr);
      const bool clockReady =
          timezoneReady && currentEpoch >= kMinimumValidEpoch;
      const codexlimits::LimitsSnapshot *const snapshot =
          store.hasData() ? &store.snapshot() : nullptr;
      dashboard.show(snapshot, connected,
                     connected && store.hasData() &&
                         !store.lastFetchFailed(),
                     fresh, store.ageSeconds(currentMs), clockReady);
      lastRedrawMs = currentMs;
      redraw = false;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
