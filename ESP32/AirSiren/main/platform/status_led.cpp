#include "platform/status_led.h"

#include "led_strip.h"

namespace alertsiren {
namespace {
constexpr int kRgbLedGpio = 8;
constexpr uint8_t kBrightness = 32;
}  // namespace

esp_err_t StatusLed::begin() {
  led_strip_handle_t strip = nullptr;
  led_strip_config_t stripConfig = {};
  stripConfig.strip_gpio_num = kRgbLedGpio;
  stripConfig.max_leds = 1;
  stripConfig.led_model = LED_MODEL_WS2812;
  stripConfig.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB;
  led_strip_rmt_config_t rmtConfig = {};
  rmtConfig.clk_src = RMT_CLK_SRC_DEFAULT;
  rmtConfig.resolution_hz = 10000000;
  rmtConfig.mem_block_symbols = 64;
  rmtConfig.flags.with_dma = false;
  const esp_err_t result =
      led_strip_new_rmt_device(&stripConfig, &rmtConfig, &strip);
  if (result == ESP_OK) {
    _handle = strip;
    return show(AlertState::Startup);
  }
  return result;
}

esp_err_t StatusLed::show(const AlertState state) {
  if (_handle == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  const OutputIntent output = outputsFor(state);
  auto *const strip = static_cast<led_strip_handle_t>(_handle);
  esp_err_t result = led_strip_set_pixel(
      strip, 0, output.redLed ? kBrightness : 0,
      output.greenLed ? kBrightness : 0, 0);
  if (result == ESP_OK) {
    result = led_strip_refresh(strip);
  }
  return result;
}

}  // namespace alertsiren
