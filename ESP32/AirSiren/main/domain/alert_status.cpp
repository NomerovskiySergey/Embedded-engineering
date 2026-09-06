#include "domain/alert_status.h"

namespace alertsiren {

OutputIntent outputsFor(const AlertState state) {
  switch (state) {
    case AlertState::Alert:
      return {true, false, UiTheme::Red};
    case AlertState::Clear:
      return {false, true, UiTheme::Green};
    case AlertState::Startup:
    case AlertState::Unknown:
    case AlertState::Stale:
      return {false, false, UiTheme::Amber};
  }
  return {false, false, UiTheme::Amber};
}

AlertState withFreshness(const AlertState state, const uint32_t lastSuccessMs,
                         const uint32_t nowMs,
                         const uint32_t freshnessMs) {
  if ((state == AlertState::Clear || state == AlertState::Alert) &&
      static_cast<uint32_t>(nowMs - lastSuccessMs) >= freshnessMs) {
    return AlertState::Stale;
  }
  return state;
}

AlertState effectiveState(const AlertState freshState, const bool connected) {
  if (!connected &&
      (freshState == AlertState::Clear || freshState == AlertState::Alert)) {
    return AlertState::Stale;
  }
  return freshState;
}

}  // namespace alertsiren
