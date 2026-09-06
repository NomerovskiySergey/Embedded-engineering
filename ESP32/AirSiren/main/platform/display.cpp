#include "platform/display.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "domain/backlight_policy.h"
#include "platform/waveshare_display_profile.h"

namespace alertsiren {
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
constexpr uint16_t kMuted = 0xBDF7;

esp_err_t applyVendorInit(const esp_lcd_panel_io_handle_t io) {
  const auto &profile = waveshareDisplayProfile();
  for (std::size_t index = 0; index < profile.commandCount; ++index) {
    const auto &entry = profile.commands[index];
    const esp_err_t result = esp_lcd_panel_io_tx_param(
        io, entry.command, entry.parameters, entry.parameterCount);
    if (result != ESP_OK) return result;
    if (entry.delayMs > 0) {
      vTaskDelay(pdMS_TO_TICKS(entry.delayMs));
    }
  }
  return ESP_OK;
}

uint16_t rgb(const uint8_t red, const uint8_t green, const uint8_t blue) {
  const uint16_t value = static_cast<uint16_t>(((red & 0xF8U) << 8U) |
                                                ((green & 0xFCU) << 3U) |
                                                (blue >> 3U));
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
    case '.': return {0x00, 0x60, 0x60, 0x00, 0x00};
    case '-': return {0x08, 0x08, 0x08, 0x08, 0x08};
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
  channelConfig.duty = kDayBacklightDuty;
  channelConfig.hpoint = 0;
  result = ledc_channel_config(&channelConfig);
  if (result != ESP_OK) return result;
  _backlightDuty = kDayBacklightDuty;

  _pixels = static_cast<uint16_t *>(
      heap_caps_malloc(kWidth * kHeight * sizeof(uint16_t), MALLOC_CAP_DMA));
  if (_pixels == nullptr) return ESP_ERR_NO_MEM;
  _panel = panel;
  return show(AlertState::Startup, false, 0);
}

esp_err_t Display::setBacklightDuty(const uint32_t duty) {
  if (duty > kMaximumBacklightDuty) return ESP_ERR_INVALID_ARG;
  if (duty == _backlightDuty) return ESP_OK;
  esp_err_t result =
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
  if (result != ESP_OK) return result;
  result = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  if (result == ESP_OK) _backlightDuty = duty;
  return result;
}

esp_err_t Display::show(const AlertState state, const bool wifiConnected,
                        const uint32_t ageSeconds, const ThreatType threat) {
  if (_panel == nullptr || _pixels == nullptr) return ESP_ERR_INVALID_STATE;
  const OutputIntent output = outputsFor(state);
  const uint16_t background = output.theme == UiTheme::Red
                                  ? rgb(145, 12, 24)
                                  : output.theme == UiTheme::Green
                                        ? rgb(8, 105, 55)
                                        : rgb(130, 76, 8);
  fill(_pixels, background);
  rectangle(_pixels, 0, 0, kWidth, 36, rgb(18, 24, 33));
  text(_pixels, 12, 10, "DNIPRO", 2, kWhite);
  text(_pixels, 124, 14, wifiConnected ? "WIFI" : "----", 1,
       wifiConnected ? rgb(80, 220, 150) : kMuted);

  const char *heading = "NO DATA";
  const char *subheading = "CHECK SOURCE";
  if (state == AlertState::Alert) {
    heading = "ALERT";
    subheading = "TAKE SHELTER";
  } else if (state == AlertState::Clear) {
    heading = "ALL CLEAR";
    subheading = "NO ACTIVE ALERT";
  } else if (state == AlertState::Startup) {
    heading = "STARTING";
    subheading = "CONNECTING";
  } else if (state == AlertState::Stale) {
    heading = "STALE";
    subheading = "CHECK SOURCE";
  }
  const int headingScale = std::strlen(heading) > 7 ? 2 : 3;
  text(_pixels, centeredX(heading, headingScale), 112, heading, headingScale,
       kWhite);
  text(_pixels, centeredX(subheading, 1), 157, subheading, 1, kWhite);

  if (state == AlertState::Alert) {
    char threatText[32];
    std::snprintf(threatText, sizeof(threatText), "THREAT: %s",
                  threatLabel(threat));
    text(_pixels, centeredX(threatText, 1), 195, threatText, 1, kWhite);
  }

  char age[32];
  if (state == AlertState::Startup || state == AlertState::Unknown) {
    std::snprintf(age, sizeof(age), "UPDATED: --");
  } else {
    std::snprintf(age, sizeof(age), "UPDATED: %lus",
                  static_cast<unsigned long>(ageSeconds));
  }
  text(_pixels, centeredX(age, 1), 235, age, 1, kWhite);
  rectangle(_pixels, 0, 282, kWidth, 38, rgb(18, 24, 33));
  text(_pixels, centeredX("DATA: TRYVOHA.ONLINE", 1), 298,
       "DATA: TRYVOHA.ONLINE", 1, kMuted);

  return esp_lcd_panel_draw_bitmap(
      static_cast<esp_lcd_panel_handle_t>(_panel), 0, 0, kWidth, kHeight,
      _pixels);
}

}  // namespace alertsiren
