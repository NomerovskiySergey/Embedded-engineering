#include <cstdint>
#include <cstdlib>
#include <ctime>

#include "domain/alert_status.h"
#include "domain/backlight_policy.h"
#include "domain/poll_schedule.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "platform/alert_client.h"
#include "platform/display.h"
#include "platform/status_led.h"
#include "platform/wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "air_siren";
constexpr uint32_t kPollIntervalMs = 30000U;
constexpr uint32_t kFirstRetryMs = 5000U;
constexpr uint32_t kMaximumRetryMs = 60000U;
constexpr uint32_t kFreshnessMs = 90000U;
constexpr uint32_t kThreatFreshnessMs = 60000U;
constexpr time_t kMinimumValidEpoch = 1704067200;  // 2024-01-01 UTC.

uint32_t nowMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

}  // namespace

extern "C" void app_main() {
  ESP_ERROR_CHECK(nvs_flash_init());
  const bool timezoneReady =
      setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1) == 0;
  if (timezoneReady) {
    tzset();
  } else {
    ESP_LOGW(kTag, "Kyiv timezone setup failed; keeping full brightness");
  }

  // AlertClient owns a bounded 32 KiB HTTP response buffer. Keep the long-lived
  // services in static storage so that they do not overflow ESP-IDF's main-task
  // stack before the first statement can run.
  static alertsiren::Display display;
  static alertsiren::StatusLed led;
  static alertsiren::WifiManager wifi;
  static alertsiren::AlertClient client;
  alertsiren::PollSchedule schedule(kPollIntervalMs, kFirstRetryMs,
                                    kMaximumRetryMs);

  ESP_ERROR_CHECK(display.begin());
  ESP_ERROR_CHECK(led.begin());
  if (!wifi.credentialsConfigured()) {
    ESP_LOGE(kTag, "Copy main/secrets.example.h to main/secrets.h and set Wi-Fi credentials");
  }
  ESP_ERROR_CHECK(wifi.begin());

  alertsiren::AlertState lastFreshState = alertsiren::AlertState::Unknown;
  alertsiren::ThreatType lastThreat = alertsiren::ThreatType::Unknown;
  uint32_t lastSuccessMs = 0;
  uint32_t lastThreatSuccessMs = 0;
  alertsiren::AlertState shownState = alertsiren::AlertState::Startup;
  bool shownWifi = false;
  uint32_t shownAgeBucket = 0;
  alertsiren::ThreatType shownThreat = alertsiren::ThreatType::None;
  bool backlightErrorReported = false;

  while (true) {
    const uint32_t currentMs = nowMs();
    const bool connected = wifi.isConnected();
    const time_t currentEpoch = std::time(nullptr);
    const bool clockReady = currentEpoch >= kMinimumValidEpoch;

    if (connected && clockReady && schedule.isDue(currentMs)) {
      const alertsiren::ProviderResult result = client.fetch();
      if (result.valid) {
        lastFreshState = result.state;
        lastSuccessMs = currentMs;
        schedule.recordSuccess(currentMs);
        ESP_LOGI(kTag, "Alert status updated: %s",
                 result.state == alertsiren::AlertState::Alert ? "ALERT" : "CLEAR");
      } else {
        schedule.recordFailure(currentMs);
        ESP_LOGW(kTag, "Alert API request or response failed");
      }
      const alertsiren::ThreatResult threatResult =
          client.fetchThreats(static_cast<int64_t>(currentEpoch));
      if (threatResult.valid) {
        lastThreat = threatResult.type;
        lastThreatSuccessMs = currentMs;
      } else {
        ESP_LOGW(kTag, "Threat feed request or response failed");
      }
    }

    alertsiren::AlertState currentState = alertsiren::AlertState::Startup;
    uint32_t ageSeconds = 0;
    if (lastSuccessMs != 0U) {
      const uint32_t ageMs = static_cast<uint32_t>(currentMs - lastSuccessMs);
      ageSeconds = ageMs / 1000U;
      currentState = alertsiren::withFreshness(lastFreshState, lastSuccessMs,
                                               currentMs, kFreshnessMs);
    } else if (connected && clockReady) {
      currentState = alertsiren::AlertState::Unknown;
    }
    currentState = alertsiren::effectiveState(currentState, connected);
    std::tm localTime = {};
    const bool localTimeReady = timezoneReady && clockReady &&
                                localtime_r(&currentEpoch, &localTime) != nullptr;
    const esp_err_t backlightResult = display.setBacklightDuty(
        alertsiren::backlightDuty(currentState, localTimeReady,
                                  localTime.tm_hour));
    if (backlightResult != ESP_OK) {
      if (!backlightErrorReported) {
        ESP_LOGW(kTag, "Backlight update failed (%s); monitoring continues",
                 esp_err_to_name(backlightResult));
        backlightErrorReported = true;
      }
    } else {
      backlightErrorReported = false;
    }
    const alertsiren::ThreatType currentThreat = alertsiren::visibleThreat(
        currentState, lastThreat, lastThreatSuccessMs, currentMs,
        kThreatFreshnessMs);

    const uint32_t ageBucket = ageSeconds / 10U;
    if (currentState != shownState || connected != shownWifi ||
        ageBucket != shownAgeBucket || currentThreat != shownThreat) {
      ESP_ERROR_CHECK(led.show(currentState));
      ESP_ERROR_CHECK(
          display.show(currentState, connected, ageSeconds, currentThreat));
      shownState = currentState;
      shownWifi = connected;
      shownAgeBucket = ageBucket;
      shownThreat = currentThreat;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
