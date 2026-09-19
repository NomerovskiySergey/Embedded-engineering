#pragma once

#include <cstdint>

#include "domain/limits_model.h"
#include "esp_err.h"

namespace codexlimits {

class Display {
 public:
  esp_err_t begin();
  esp_err_t show(const LimitsSnapshot *snapshot, bool wifiConnected,
                 bool bridgeHealthy, bool fresh, uint32_t ageSeconds,
                 bool clockReady);

 private:
  void *_panel = nullptr;
  uint16_t *_pixels = nullptr;
};

}  // namespace codexlimits
