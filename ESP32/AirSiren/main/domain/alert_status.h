#pragma once

#include <cstdint>

namespace alertsiren {

enum class AlertState : uint8_t { Startup, Unknown, Clear, Alert, Stale };
enum class UiTheme : uint8_t { Amber, Green, Red };

struct OutputIntent {
  bool redLed;
  bool greenLed;
  UiTheme theme;
};

OutputIntent outputsFor(AlertState state);
AlertState withFreshness(AlertState state, uint32_t lastSuccessMs,
                         uint32_t nowMs, uint32_t freshnessMs);
AlertState effectiveState(AlertState freshState, bool connected);

}  // namespace alertsiren
