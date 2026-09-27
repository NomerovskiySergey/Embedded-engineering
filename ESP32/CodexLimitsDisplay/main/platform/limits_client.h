#pragma once

#include <cstddef>

#include "domain/limits_model.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace codexlimits {

struct FetchResult {
  bool valid = false;
  LimitsSnapshot snapshot = {};
};

class LimitsClient {
 public:
  esp_err_t begin();
  bool configured() const;
  bool requestFetch();
  bool takeResult(FetchResult &result);

 private:
  static constexpr std::size_t kResponseCapacity = 4097U;
  static void taskEntry(void *context);
  void taskLoop();
  FetchResult fetch();

  char _response[kResponseCapacity] = {};
  std::size_t _responseLength = 0;
  bool _overflow = false;
  QueueHandle_t _requestQueue = nullptr;
  QueueHandle_t _resultQueue = nullptr;
  friend esp_err_t limitsHttpEventHandler(esp_http_client_event_t *);
};

}  // namespace codexlimits
