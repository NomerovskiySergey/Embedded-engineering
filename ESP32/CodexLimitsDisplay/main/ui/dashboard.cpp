#include "ui/dashboard.h"

#include <cstdio>
#include <ctime>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "platform/waveshare_display_profile.h"

namespace codexlimits {
namespace {

constexpr char kTag[] = "dashboard";
constexpr TickType_t kFlushTimeout = pdMS_TO_TICKS(1000);
static_assert(dashboard::kNativeSwappedRgb565,
              "ST7789 SPI transport requires swapped RGB565 rendering");

lv_color_t midnight() { return lv_color_make(8, 9, 12); }
lv_color_t cardBackground() { return lv_color_make(22, 26, 38); }
lv_color_t slate() { return lv_color_make(45, 52, 70); }
lv_color_t cyan() { return lv_color_make(0, 245, 255); }
lv_color_t green() { return lv_color_make(0, 255, 102); }
lv_color_t pink() { return lv_color_make(255, 0, 127); }
lv_color_t muted() { return lv_color_make(180, 185, 200); }

esp_err_t applyVendorInit(const esp_lcd_panel_io_handle_t io) {
  const auto &profile = waveshareDisplayProfile();
  for (std::size_t index = 0; index < profile.commandCount; ++index) {
    const auto &entry = profile.commands[index];
    const esp_err_t result = esp_lcd_panel_io_tx_param(
        io, entry.command, entry.parameters, entry.parameterCount);
    if (result != ESP_OK) return result;
    if (entry.delayMs > 0U) vTaskDelay(pdMS_TO_TICKS(entry.delayMs));
  }
  return ESP_OK;
}

void place(lv_obj_t *object, const dashboard::Rect &item,
           const dashboard::Rect &parent) {
  lv_obj_set_pos(object, item.x - parent.x, item.y - parent.y);
  lv_obj_set_size(object, item.width, item.height);
}

}  // namespace

esp_err_t Dashboard::begin(const uint32_t nowMs) {
  const auto &profile = waveshareDisplayProfile();
  _flushComplete = xSemaphoreCreateBinary();
  if (_flushComplete == nullptr) return ESP_ERR_NO_MEM;

  spi_bus_config_t busConfig = {};
  busConfig.mosi_io_num = static_cast<gpio_num_t>(dashboard::kMosiPin);
  busConfig.miso_io_num = GPIO_NUM_NC;
  busConfig.sclk_io_num = static_cast<gpio_num_t>(dashboard::kClockPin);
  busConfig.quadwp_io_num = GPIO_NUM_NC;
  busConfig.quadhd_io_num = GPIO_NUM_NC;
  busConfig.max_transfer_sz = sizeof(_drawBuffer) + 8;
  esp_err_t result = spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO);
  if (result != ESP_OK) return result;

  esp_lcd_panel_io_handle_t io = nullptr;
  esp_lcd_panel_io_spi_config_t ioConfig = {};
  ioConfig.cs_gpio_num =
      static_cast<gpio_num_t>(dashboard::kChipSelectPin);
  ioConfig.dc_gpio_num =
      static_cast<gpio_num_t>(dashboard::kDataCommandPin);
  ioConfig.spi_mode = 0;
  ioConfig.pclk_hz = dashboard::kPixelClockHz;
  ioConfig.trans_queue_depth = 1;
  ioConfig.on_color_trans_done = &Dashboard::transferDoneCallback;
  ioConfig.user_ctx = this;
  ioConfig.lcd_cmd_bits = 8;
  ioConfig.lcd_param_bits = 8;
  result = esp_lcd_new_panel_io_spi(SPI2_HOST, &ioConfig, &io);
  if (result != ESP_OK) return result;

  esp_lcd_panel_handle_t panel = nullptr;
  esp_lcd_panel_dev_config_t panelConfig = {};
  panelConfig.reset_gpio_num =
      static_cast<gpio_num_t>(dashboard::kResetPin);
  panelConfig.rgb_ele_order =
      profile.bgrOrder ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB;
  panelConfig.bits_per_pixel = 16;
  result = esp_lcd_new_panel_st7789(io, &panelConfig, &panel);
  if (result != ESP_OK) return result;
  if ((result = esp_lcd_panel_reset(panel)) != ESP_OK) return result;
  if ((result = esp_lcd_panel_init(panel)) != ESP_OK) return result;
  if ((result = applyVendorInit(io)) != ESP_OK) return result;
  if ((result = esp_lcd_panel_swap_xy(panel, dashboard::kSwapAxes)) !=
      ESP_OK) {
    return result;
  }
  if ((result = esp_lcd_panel_mirror(panel, dashboard::kMirrorX,
                                     dashboard::kMirrorY)) != ESP_OK) {
    return result;
  }
  if ((result = esp_lcd_panel_set_gap(panel, dashboard::kPanelXGap,
                                      dashboard::kPanelYGap)) != ESP_OK) {
    return result;
  }

  gpio_config_t backlight = {};
  backlight.pin_bit_mask = 1ULL << dashboard::kBacklightPin;
  backlight.mode = GPIO_MODE_OUTPUT;
  backlight.pull_up_en = GPIO_PULLUP_DISABLE;
  backlight.pull_down_en = GPIO_PULLDOWN_DISABLE;
  backlight.intr_type = GPIO_INTR_DISABLE;
  if ((result = gpio_config(&backlight)) != ESP_OK) return result;
  if ((result = gpio_set_level(
           static_cast<gpio_num_t>(dashboard::kBacklightPin), 1)) != ESP_OK) {
    return result;
  }

  _panel = panel;
  lv_init();
  _display =
      lv_display_create(dashboard::kScreenWidth, dashboard::kScreenHeight);
  if (_display == nullptr) return ESP_ERR_NO_MEM;
  lv_display_set_color_format(_display, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_user_data(_display, this);
  lv_display_set_flush_cb(_display, &Dashboard::flushCallback);
  lv_display_set_buffers(_display, _drawBuffer, nullptr, sizeof(_drawBuffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  create_codex_limits_ui();
  _lastServiceMs = nowMs;
  _ready = true;
  return ESP_OK;
}

bool Dashboard::transferDoneCallback(
    esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *context) {
  auto *const self = static_cast<Dashboard *>(context);
  if (self == nullptr || self->_flushComplete == nullptr) return false;
  BaseType_t taskWoken = pdFALSE;
  xSemaphoreGiveFromISR(self->_flushComplete, &taskWoken);
  return taskWoken == pdTRUE;
}

void Dashboard::flushCallback(lv_display_t *const display,
                              const lv_area_t *const area,
                              uint8_t *const pixelMap) {
  auto *const self =
      static_cast<Dashboard *>(lv_display_get_user_data(display));
  const dashboard::FlushRegion region =
      area == nullptr
          ? dashboard::FlushRegion{0, 0, 0, 0, false}
          : dashboard::makeFlushRegion(area->x1, area->y1, area->x2, area->y2);
  if (self != nullptr && self->_panel != nullptr && pixelMap != nullptr &&
      region.valid) {
    xSemaphoreTake(self->_flushComplete, 0);
    const esp_err_t result = esp_lcd_panel_draw_bitmap(
        static_cast<esp_lcd_panel_handle_t>(self->_panel), region.x, region.y,
        region.x + region.width, region.y + region.height, pixelMap);
    if (result != ESP_OK) {
      ESP_LOGE(kTag, "LCD flush failed: %s", esp_err_to_name(result));
    } else if (xSemaphoreTake(self->_flushComplete, kFlushTimeout) != pdTRUE) {
      ESP_LOGE(kTag, "LCD flush timed out");
    }
  }
  lv_display_flush_ready(display);
}

void Dashboard::createCard(const dashboard::Rect &cardRect,
                           const dashboard::Rect &titleRect,
                           const dashboard::Rect &valueRect,
                           const dashboard::Rect &barRect,
                           const dashboard::Rect &footerRect,
                           CardWidgets &widgets) {
  lv_obj_t *const screen = lv_screen_active();
  lv_obj_t *const card = lv_obj_create(screen);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(card, cardRect.x, cardRect.y);
  lv_obj_set_size(card, cardRect.width, cardRect.height);
  lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(card, cardBackground(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_90, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, slate(), LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);

  widgets.title = lv_label_create(card);
  place(widgets.title, titleRect, cardRect);
  lv_label_set_long_mode(widgets.title, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(widgets.title, &lv_font_montserrat_10,
                             LV_PART_MAIN);
  lv_obj_set_style_text_color(widgets.title, muted(), LV_PART_MAIN);

  widgets.value = lv_label_create(card);
  place(widgets.value, valueRect, cardRect);
  lv_label_set_long_mode(widgets.value, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(widgets.value, &lv_font_montserrat_16,
                             LV_PART_MAIN);
  lv_obj_set_style_text_color(widgets.value, lv_color_white(), LV_PART_MAIN);

  widgets.bar = lv_bar_create(card);
  place(widgets.bar, barRect, cardRect);
  lv_bar_set_range(widgets.bar, 0, 100);
  lv_obj_set_style_radius(widgets.bar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(widgets.bar, 4, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(widgets.bar, slate(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(widgets.bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(widgets.bar, cyan(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(widgets.bar, LV_OPA_COVER, LV_PART_INDICATOR);

  widgets.footer = lv_label_create(card);
  place(widgets.footer, footerRect, cardRect);
  lv_label_set_long_mode(widgets.footer, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(widgets.footer, &lv_font_montserrat_10,
                             LV_PART_MAIN);
  lv_obj_set_style_text_color(widgets.footer, muted(), LV_PART_MAIN);
}

void Dashboard::create_codex_limits_ui() {
  lv_obj_t *const screen = lv_screen_active();
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, midnight(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);

  lv_obj_t *const header = lv_obj_create(screen);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(header, dashboard::kHeader.x, dashboard::kHeader.y);
  lv_obj_set_size(header, dashboard::kHeader.width, dashboard::kHeader.height);
  lv_obj_set_style_bg_color(header, midnight(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);

  lv_obj_t *const heading = lv_label_create(header);
  lv_label_set_text(heading, "[!] CODEX ACCESS LIMITS");
  lv_obj_set_pos(heading, 8, 8);
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_color(heading, pink(), LV_PART_MAIN);

  _statusLabel = lv_label_create(header);
  lv_obj_set_pos(_statusLabel, 232, 8);
  lv_obj_set_size(_statusLabel, 80, 12);
  lv_obj_set_style_text_align(_statusLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_obj_set_style_text_font(_statusLabel, &lv_font_montserrat_10,
                             LV_PART_MAIN);

  createCard(dashboard::kLeftCard, dashboard::kLeftTitle,
             dashboard::kLeftValue, dashboard::kLeftBar,
             dashboard::kLeftFooter, _left);
  createCard(dashboard::kRightCard, dashboard::kRightTitle,
             dashboard::kRightValue, dashboard::kRightBar,
             dashboard::kRightFooter, _right);
  update(dashboard::initialDashboardData());
}

void Dashboard::update(const dashboard::DashboardData &data) {
  if (_left.title == nullptr || _right.title == nullptr) return;
  lv_label_set_text(_left.title, data.left.title);
  lv_label_set_text(_left.value, data.left.value);
  lv_label_set_text(_left.footer, data.left.footer);
  lv_bar_set_value(_left.bar, dashboard::clampPercent(data.left.percent),
                   LV_ANIM_OFF);
  lv_label_set_text(_right.title, data.right.title);
  lv_label_set_text(_right.value, data.right.value);
  lv_label_set_text(_right.footer, data.right.footer);
  lv_bar_set_value(_right.bar, dashboard::clampPercent(data.right.percent),
                   LV_ANIM_OFF);
  lv_label_set_text(_statusLabel,
                    data.systemActive ? LV_SYMBOL_CHARGE " ACTIVE"
                                      : "[!] STALE");
  lv_obj_set_style_text_color(_statusLabel,
                              data.systemActive ? green() : pink(),
                              LV_PART_MAIN);
}

void Dashboard::show(const LimitsSnapshot *const snapshot,
                     const bool wifiConnected, const bool bridgeHealthy,
                     const bool fresh, const uint32_t ageSeconds,
                     const bool clockReady) {
  dashboard::DashboardData data = dashboard::initialDashboardData();
  data.systemActive = wifiConnected && bridgeHealthy && fresh;
  if (snapshot == nullptr || snapshot->count == 0U) {
    update(data);
    return;
  }

  char leftTitle[24] = {};
  char leftValue[24] = {};
  char leftFooter[32] = {};
  char rightTitle[24] = {};
  char rightValue[24] = {};
  char rightFooter[32] = {};

  const auto formatCard = [&](const LimitWindow &window, char *title,
                              const std::size_t titleSize, char *value,
                              const std::size_t valueSize, char *footer,
                              const std::size_t footerSize) {
    std::snprintf(title, titleSize, "%s WINDOW", window.label);
    std::snprintf(value, valueSize, "%u%% USED",
                  static_cast<unsigned>(window.usedPercent));
    if (clockReady) {
      const time_t epoch = static_cast<time_t>(window.resetsAt);
      std::tm local = {};
      if (localtime_r(&epoch, &local) != nullptr) {
        std::snprintf(footer, footerSize, "Reset: %02d:%02d",
                      local.tm_hour, local.tm_min);
        return;
      }
    }
    std::snprintf(footer, footerSize, "Updated: %lus ago",
                  static_cast<unsigned long>(ageSeconds));
  };

  formatCard(snapshot->windows[0], leftTitle, sizeof(leftTitle), leftValue,
             sizeof(leftValue), leftFooter, sizeof(leftFooter));
  data.left = {leftTitle, leftValue, leftFooter,
               dashboard::clampPercent(snapshot->windows[0].usedPercent)};

  if (snapshot->count > 1U) {
    formatCard(snapshot->windows[1], rightTitle, sizeof(rightTitle), rightValue,
               sizeof(rightValue), rightFooter, sizeof(rightFooter));
    data.right = {rightTitle, rightValue, rightFooter,
                  dashboard::clampPercent(snapshot->windows[1].usedPercent)};
  } else {
    data.right = {"SECONDARY LIMIT", "--", "Not available", 0U};
  }
  update(data);
}

void Dashboard::service(const uint32_t nowMs) {
  if (!_ready ||
      !dashboard::serviceDue(_lastServiceMs, nowMs,
                             dashboard::kLvglServiceIntervalMs)) {
    return;
  }
  const uint32_t elapsed = static_cast<uint32_t>(nowMs - _lastServiceMs);
  _lastServiceMs = nowMs;
  lv_tick_inc(elapsed);
  lv_timer_handler();
}

}  // namespace codexlimits
