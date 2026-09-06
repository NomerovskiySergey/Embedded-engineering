#include "domain/threat_status.h"

namespace alertsiren {

ThreatType visibleThreat(const AlertState alertState, const ThreatType threat,
                         const uint32_t lastSuccessMs, const uint32_t nowMs,
                         const uint32_t freshnessMs) {
  if (alertState != AlertState::Alert) return ThreatType::None;
  if (lastSuccessMs == 0U ||
      static_cast<uint32_t>(nowMs - lastSuccessMs) >= freshnessMs) {
    return ThreatType::Unknown;
  }
  return threat;
}

const char *threatLabel(const ThreatType threat) {
  switch (threat) {
    case ThreatType::Missile: return "MISSILE";
    case ThreatType::GuidedBomb: return "KAB";
    case ThreatType::StrikeDrone: return "DRONE";
    case ThreatType::ReconDrone: return "RECON DRONE";
    case ThreatType::Warning: return "WARNING";
    case ThreatType::Other: return "OTHER";
    case ThreatType::Multiple: return "MULTIPLE";
    case ThreatType::Unknown: return "UNKNOWN";
    case ThreatType::None: return "";
  }
  return "UNKNOWN";
}

}  // namespace alertsiren
