#pragma once

#include "domain/alert_status.h"
#include "esp_err.h"

namespace alertsiren {

class StatusLed {
 public:
  esp_err_t begin();
  esp_err_t show(AlertState state);

 private:
  void *_handle = nullptr;
};

}  // namespace alertsiren
