#pragma once

#include <cstddef>

#include "esp_err.h"
#include "esp_http_client.h"
#include "providers/tryvoha_parser.h"
#include "providers/tryvoha_events_parser.h"

namespace alertsiren {

class AlertClient {
 public:
  ProviderResult fetch();
  ThreatResult fetchThreats(int64_t nowEpochSeconds);

 private:
  static constexpr size_t kResponseCapacity = 32769;
  bool fetchPayload(const char *url);
  char _response[kResponseCapacity] = {};
  size_t _responseLength = 0;
  bool _overflow = false;
  friend esp_err_t httpEventHandler(esp_http_client_event_t *);
};

}  // namespace alertsiren
