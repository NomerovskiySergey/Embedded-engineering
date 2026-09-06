#pragma once

#include <cstdint>

#include "domain/alert_status.h"

namespace alertsiren {

enum class ThreatType : uint8_t {
  None,
  Unknown,
  Missile,
  GuidedBomb,
  StrikeDrone,
  ReconDrone,
  Warning,
  Other,
  Multiple
};

ThreatType visibleThreat(AlertState alertState, ThreatType threat,
                         uint32_t lastSuccessMs, uint32_t nowMs,
                         uint32_t freshnessMs);
const char *threatLabel(ThreatType threat);

}  // namespace alertsiren
