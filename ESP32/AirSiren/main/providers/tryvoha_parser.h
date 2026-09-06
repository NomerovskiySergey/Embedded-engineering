#pragma once

#include <string_view>

#include "domain/alert_status.h"

namespace alertsiren {

struct ProviderResult {
  bool valid;
  AlertState state;
};

ProviderResult parseTryvohaResponse(std::string_view payload);

}  // namespace alertsiren
