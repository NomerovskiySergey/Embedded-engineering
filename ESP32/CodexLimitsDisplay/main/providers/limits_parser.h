#pragma once

#include <string_view>

#include "domain/limits_model.h"

namespace codexlimits {

struct ParseResult {
  bool valid;
  LimitsSnapshot snapshot;
};

ParseResult parseLimitsPayload(std::string_view payload);

}  // namespace codexlimits
