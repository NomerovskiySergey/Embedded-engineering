#include "domain/backlight_policy.h"

namespace alertsiren {

uint32_t backlightDuty(const AlertState state, const bool clockValid,
                       const int localHour) {
  if (!clockValid || localHour < 0 || localHour > 23 ||
      state == AlertState::Alert) {
    return kDayBacklightDuty;
  }
  const bool night = localHour >= 23 || localHour < 7;
  return night ? kNightBacklightDuty : kDayBacklightDuty;
}

}  // namespace alertsiren
