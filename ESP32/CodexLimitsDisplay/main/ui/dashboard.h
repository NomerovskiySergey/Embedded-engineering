#pragma once

#include <cstdint>

#include "domain/limits_model.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include "ui/dashboard_model.h"

namespace codexlimits {

class Dashboard {
 public:
  esp_err_t begin(uint32_t nowMs);
  void service(uint32_t nowMs);
  void update(const dashboard::DashboardData &data);
  void show(const LimitsSnapshot *snapshot, bool wifiConnected,
            bool bridgeHealthy, bool fresh, uint32_t ageSeconds,
            bool clockReady);

 private:
  struct CardWidgets {
    lv_obj_t *title = nullptr;
    lv_obj_t *value = nullptr;
    lv_obj_t *bar = nullptr;
    lv_obj_t *footer = nullptr;
  };

  static void flushCallback(lv_display_t *display, const lv_area_t *area,
                            uint8_t *pixelMap);
  static bool transferDoneCallback(esp_lcd_panel_io_handle_t io,
                                   esp_lcd_panel_io_event_data_t *eventData,
                                   void *context);
  void create_codex_limits_ui();
  void createCard(const dashboard::Rect &cardRect,
                  const dashboard::Rect &titleRect,
                  const dashboard::Rect &valueRect,
                  const dashboard::Rect &barRect,
                  const dashboard::Rect &footerRect,
                  CardWidgets &widgets);
  void *_panel = nullptr;
  SemaphoreHandle_t _flushComplete = nullptr;
  lv_display_t *_display = nullptr;
  lv_obj_t *_statusLabel = nullptr;
  CardWidgets _left = {};
  CardWidgets _right = {};
  alignas(4) uint16_t _drawBuffer[dashboard::kScreenWidth * 20] = {};
  uint32_t _lastServiceMs = 0U;
  bool _ready = false;
};

}  // namespace codexlimits
