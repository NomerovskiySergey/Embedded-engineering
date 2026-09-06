#pragma once

#include <cstdint>

#include "domain/alert_status.h"
#include "domain/threat_status.h"
#include "esp_err.h"

namespace alertsiren {

class Display {
 public:
  esp_err_t begin();
  esp_err_t show(AlertState state, bool wifiConnected,
                 uint32_t ageSeconds = 0,
                 ThreatType threat = ThreatType::None);
  esp_err_t setBacklightDuty(uint32_t duty);

 private:
  void *_panel = nullptr;
  uint16_t *_pixels = nullptr;
  uint32_t _backlightDuty = 0;
};

}  // namespace alertsiren
