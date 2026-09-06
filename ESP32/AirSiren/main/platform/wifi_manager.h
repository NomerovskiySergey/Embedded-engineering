#pragma once

#include <atomic>

#include "esp_err.h"

namespace alertsiren {

class WifiManager {
 public:
  esp_err_t begin();
  bool isConnected() const;
  bool credentialsConfigured() const;

 private:
  std::atomic_bool _connected{false};
  friend void wifiEventHandler(void *, const char *, int32_t, void *);
};

}  // namespace alertsiren
