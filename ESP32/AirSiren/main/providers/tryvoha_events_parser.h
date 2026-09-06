#pragma once

#include <string_view>

#include "domain/threat_status.h"

namespace alertsiren {

struct ThreatResult {
  bool valid;
  ThreatType type;
};

ThreatResult parseTryvohaEvents(std::string_view payload,
                                int64_t nowEpochSeconds = 0);

}  // namespace alertsiren
