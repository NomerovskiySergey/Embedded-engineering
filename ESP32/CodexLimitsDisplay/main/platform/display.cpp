#include "platform/display.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/waveshare_display_profile.h"

namespace codexlimits {
namespace {

constexpr int kWidth = 172;
constexpr int kHeight = 320;
constexpr gpio_num_t kMosi = GPIO_NUM_6;
constexpr gpio_num_t kClock = GPIO_NUM_7;
constexpr gpio_num_t kChipSelect = GPIO_NUM_14;
constexpr gpio_num_t kDataCommand = GPIO_NUM_15;
constexpr gpio_num_t kReset = GPIO_NUM_21;
constexpr gpio_num_t kBacklight = GPIO_NUM_22;
constexpr uint16_t kWhite = 0xFFFF;

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

uint16_t rgb(const uint8_t red, const uint8_t green, const uint8_t blue) {
  const uint16_t value = static_cast<uint16_t>(
      ((red & 0xF8U) << 8U) | ((green & 0xFCU) << 3U) | (blue >> 3U));
  return static_cast<uint16_t>((value << 8U) | (value >> 8U));
}

std::array<uint8_t, 5> glyph(const char character) {
  switch (character) {
    case 'A': return {0x7E, 0x11, 0x11, 0x11, 0x7E};
    case 'B': return {0x7F, 0x49, 0x49, 0x49, 0x36};
    case 'C': return {0x3E, 0x41, 0x41, 0x41, 0x22};
    case 'D': return {0x7F, 0x41, 0x41, 0x22, 0x1C};
    case 'E': return {0x7F, 0x49, 0x49, 0x49, 0x41};
    case 'F': return {0x7F, 0x09, 0x09, 0x09, 0x01};
    case 'G': return {0x3E, 0x41, 0x49, 0x49, 0x7A};
    case 'H': return {0x7F, 0x08, 0x08, 0x08, 0x7F};
    case 'I': return {0x00, 0x41, 0x7F, 0x41, 0x00};
    case 'J': return {0x20, 0x40, 0x41, 0x3F, 0x01};
    case 'K': return {0x7F, 0x08, 0x14, 0x22, 0x41};
    case 'L': return {0x7F, 0x40, 0x40, 0x40, 0x40};
    case 'M': return {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    case 'N': return {0x7F, 0x04, 0x08, 0x10, 0x7F};
    case 'O': return {0x3E, 0x41, 0x41, 0x41, 0x3E};
    case 'P': return {0x7F, 0x09, 0x09, 0x09, 0x06};
    case 'Q': return {0x3E, 0x41, 0x51, 0x21, 0x5E};
    case 'R': return {0x7F, 0x09, 0x19, 0x29, 0x46};
    case 'S': return {0x46, 0x49, 0x49, 0x49, 0x31};
    case 'T': return {0x01, 0x01, 0x7F, 0x01, 0x01};
    case 'U': return {0x3F, 0x40, 0x40, 0x40, 0x3F};
    case 'V': return {0x1F, 0x20, 0x40, 0x20, 0x1F};
    case 'W': return {0x3F, 0x40, 0x38, 0x40, 0x3F};
    case 'X': return {0x63, 0x14, 0x08, 0x14, 0x63};
    case 'Y': return {0x07, 0x08, 0x70, 0x08, 0x07};
    case 'Z': return {0x61, 0x51, 0x49, 0x45, 0x43};
    case '0': return {0x3E, 0x51, 0x49, 0x45, 0x3E};
    case '1': return {0x00, 0x42, 0x7F, 0x40, 0x00};
    case '2': return {0x42, 0x61, 0x51, 0x49, 0x46};
    case '3': return {0x21, 0x41, 0x45, 0x4B, 0x31};
    case '4': return {0x18, 0x14, 0x12, 0x7F, 0x10};
    case '5': return {0x27, 0x45, 0x45, 0x45, 0x39};
    case '6': return {0x3C, 0x4A, 0x49, 0x49, 0x30};
    case '7': return {0x01, 0x71, 0x09, 0x05, 0x03};
    case '8': return {0x36, 0x49, 0x49, 0x49, 0x36};
    case '9': return {0x06, 0x49, 0x49, 0x29, 0x1E};
    case ':': return {0x00, 0x36, 0x36, 0x00, 0x00};
    case '-': return {0x08, 0x08, 0x08, 0x08, 0x08};
    case '%': return {0x63, 0x13, 0x08, 0x64, 0x63};
    default: return {0, 0, 0, 0, 0};
  }
}

void fill(uint16_t *pixels, const uint16_t color) {
  std::fill_n(pixels, kWidth * kHeight, color);
}

void rectangle(uint16_t *pixels, int x, int y, int width, int height,
               const uint16_t color) {
  const int endX = std::min(x + width, kWidth);
  const int endY = std::min(y + height, kHeight);
  x = std::max(x, 0);
  y = std::max(y, 0);
  for (int row = y; row < endY; ++row) {
    std::fill(pixels + row * kWidth + x, pixels + row * kWidth + endX, color);
  }
}

void text(uint16_t *pixels, int x, const int y, const char *value,
          const int scale, const uint16_t color) {
  for (; *value != '\0'; ++value, x += 6 * scale) {
    const auto columns = glyph(*value);
    for (int column = 0; column < 5; ++column) {
      for (int row = 0; row < 7; ++row) {
        if ((columns[column] & (1U << row)) != 0U) {
          rectangle(pixels, x + column * scale, y + row * scale, scale, scale,
                    color);
        }
      }
    }
  }
}

int centeredX(const char *value, const int scale) {
  return std::max(0, (kWidth - static_cast<int>(std::strlen(value)) * 6 * scale +
                      scale) /
                         2);
}

uint16_t colorFor(const UsageColor color) {
  if (color == UsageColor::Red) return rgb(235, 70, 70);
  if (color == UsageColor::Amber) return rgb(245, 176, 65);
  return rgb(70, 210, 135);
}

void drawWindow(uint16_t *pixels, const LimitWindow &window, const int y,
                const bool fresh, const bool clockReady) {
  const uint16_t accent =
      fresh ? colorFor(usageColor(window.usedPercent)) : rgb(120, 132, 148);
  char value[24] = {};
  std::snprintf(value, sizeof(value), "%u%% LEFT",
                static_cast<unsigned>(remainingPercent(window.usedPercent)));
  text(pixels, 12, y, window.label, 2, kWhite);
  text(pixels, 62, y + 5, value, 1, accent);
  rectangle(pixels, 12, y + 26, 148, 18, rgb(35, 43, 57));
  const int usedWidth =
      static_cast<int>((148U * window.usedPercent) / 100U);
  rectangle(pixels, 12, y + 26, usedWidth, 18, accent);
  char reset[32] = "RESET --:--";
  if (clockReady) {
    const time_t epoch = static_cast<time_t>(window.resetsAt);
    std::tm local = {};
    if (localtime_r(&epoch, &local) != nullptr) {
      std::snprintf(reset, sizeof(reset), "RESET %02d:%02d",
                    local.tm_hour, local.tm_min);
    }
  }
  text(pixels, 12, y + 51, reset, 1, rgb(170, 182, 199));
}

}  // namespace

esp_err_t Display::begin() {
  const auto &profile = waveshareDisplayProfile();
  spi_bus_config_t busConfig = {};
  busConfig.mosi_io_num = kMosi;
  busConfig.miso_io_num = GPIO_NUM_NC;
  busConfig.sclk_io_num = kClock;
  busConfig.quadwp_io_num = GPIO_NUM_NC;
  busConfig.quadhd_io_num = GPIO_NUM_NC;
  busConfig.max_transfer_sz = kWidth * kHeight * 2;
  esp_err_t result = spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO);
  if (result != ESP_OK) return result;
  esp_lcd_panel_io_handle_t io = nullptr;
  esp_lcd_panel_io_spi_config_t ioConfig = {};
  ioConfig.cs_gpio_num = kChipSelect;
  ioConfig.dc_gpio_num = kDataCommand;
  ioConfig.spi_mode = 0;
  ioConfig.pclk_hz = profile.pixelClockHz;
  ioConfig.trans_queue_depth = 4;
  ioConfig.lcd_cmd_bits = 8;
  ioConfig.lcd_param_bits = 8;
  result = esp_lcd_new_panel_io_spi(SPI2_HOST, &ioConfig, &io);
  if (result != ESP_OK) return result;
  esp_lcd_panel_handle_t panel = nullptr;
  esp_lcd_panel_dev_config_t panelConfig = {};
  panelConfig.reset_gpio_num = kReset;
  panelConfig.rgb_ele_order = profile.bgrOrder ? LCD_RGB_ELEMENT_ORDER_BGR
                                               : LCD_RGB_ELEMENT_ORDER_RGB;
  panelConfig.bits_per_pixel = 16;
  result = esp_lcd_new_panel_st7789(io, &panelConfig, &panel);
  if (result != ESP_OK) return result;
  result = esp_lcd_panel_reset(panel);
  if (result != ESP_OK) return result;
  result = esp_lcd_panel_init(panel);
  if (result != ESP_OK) return result;
  result = applyVendorInit(io);
  if (result != ESP_OK) return result;
  result = esp_lcd_panel_mirror(panel, profile.mirrorX, profile.mirrorY);
  if (result != ESP_OK) return result;
  result = esp_lcd_panel_set_gap(panel, profile.xGap, profile.yGap);
  if (result != ESP_OK) return result;
  ledc_timer_config_t timerConfig = {};
  timerConfig.speed_mode = LEDC_LOW_SPEED_MODE;
  timerConfig.duty_resolution = LEDC_TIMER_10_BIT;
  timerConfig.timer_num = LEDC_TIMER_0;
  timerConfig.freq_hz = 5000;
  timerConfig.clk_cfg = LEDC_AUTO_CLK;
  result = ledc_timer_config(&timerConfig);
  if (result != ESP_OK) return result;
  ledc_channel_config_t channelConfig = {};
  channelConfig.gpio_num = kBacklight;
  channelConfig.speed_mode = LEDC_LOW_SPEED_MODE;
  channelConfig.channel = LEDC_CHANNEL_0;
  channelConfig.timer_sel = LEDC_TIMER_0;
  channelConfig.duty = 409U;
  channelConfig.hpoint = 0;
  result = ledc_channel_config(&channelConfig);
  if (result != ESP_OK) return result;
  _pixels = static_cast<uint16_t *>(
      heap_caps_malloc(kWidth * kHeight * sizeof(uint16_t), MALLOC_CAP_DMA));
  if (_pixels == nullptr) return ESP_ERR_NO_MEM;
  _panel = panel;
  return show(nullptr, false, false, false, 0U, false);
}

esp_err_t Display::show(const LimitsSnapshot *const snapshot,
                        const bool wifiConnected, const bool bridgeHealthy,
                        const bool fresh, const uint32_t ageSeconds,
                        const bool clockReady) {
  if (_panel == nullptr || _pixels == nullptr) return ESP_ERR_INVALID_STATE;
  fill(_pixels, rgb(13, 18, 28));
  rectangle(_pixels, 0, 0, kWidth, 36, rgb(28, 36, 50));
  text(_pixels, 8, 11, "CODEX LIMITS", 1, kWhite);
  text(_pixels, 140, 11, wifiConnected ? "WIFI" : "----", 1,
       wifiConnected ? rgb(70, 210, 135) : rgb(135, 145, 160));
  if (snapshot == nullptr || snapshot->count == 0U) {
    const char *heading = wifiConnected ? "NO DATA" : "NO WIFI";
    const char *detail = wifiConnected ? "START BRIDGE" : "CONNECTING";
    text(_pixels, centeredX(heading, 2), 118, heading, 2, kWhite);
    text(_pixels, centeredX(detail, 1), 156, detail, 1,
         rgb(170, 182, 199));
  } else {
    for (uint8_t index = 0U; index < snapshot->count; ++index) {
      drawWindow(_pixels, snapshot->windows[index],
                 56 + static_cast<int>(index) * 96, fresh, clockReady);
    }
    if (snapshot->truncated) {
      text(_pixels, 12, 252, "MORE WINDOWS", 1, rgb(170, 182, 199));
    }
  }
  rectangle(_pixels, 0, 282, kWidth, 38, rgb(28, 36, 50));
  char status[32] = {};
  if (!wifiConnected) {
    std::snprintf(status, sizeof(status), "NO WIFI");
  } else if (!bridgeHealthy && snapshot != nullptr) {
    std::snprintf(status, sizeof(status), "BRIDGE ERROR");
  } else if (snapshot == nullptr) {
    std::snprintf(status, sizeof(status), "WAITING");
  } else {
    std::snprintf(status, sizeof(status), "%s %lus",
                  fresh ? "FRESH" : "STALE",
                  static_cast<unsigned long>(ageSeconds));
  }
  text(_pixels, centeredX(status, 1), 298, status, 1,
       fresh && bridgeHealthy ? rgb(70, 210, 135) : rgb(245, 176, 65));
  return esp_lcd_panel_draw_bitmap(
      static_cast<esp_lcd_panel_handle_t>(_panel), 0, 0, kWidth, kHeight,
      _pixels);
}

}  // namespace codexlimits
