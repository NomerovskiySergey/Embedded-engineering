#pragma once

#include <cstdint>

#include "domain/alert_status.h"

namespace alertsiren {

constexpr uint32_t kMaximumBacklightDuty = 1023U;
constexpr uint32_t kDayBacklightDuty = 409U;
constexpr uint32_t kNightBacklightDuty = 82U;

uint32_t backlightDuty(AlertState state, bool clockValid, int localHour);

}  // namespace alertsiren
