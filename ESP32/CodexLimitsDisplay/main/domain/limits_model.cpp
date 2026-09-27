#include "domain/limits_model.h"

#include <algorithm>

namespace codexlimits {

uint8_t remainingPercent(const uint32_t usedPercent) {
  return static_cast<uint8_t>(
      static_cast<uint32_t>(100U) -
      std::min(usedPercent, static_cast<uint32_t>(100U)));
}

UsageColor usageColor(const uint8_t usedPercent) {
  if (usedPercent >= 90U) return UsageColor::Red;
  if (usedPercent >= 70U) return UsageColor::Amber;
  return UsageColor::Green;
}

bool isFresh(const uint32_t updatedMs, const uint32_t nowMs,
             const uint32_t freshnessMs) {
  return static_cast<uint32_t>(nowMs - updatedMs) < freshnessMs;
}

bool displayDataFresh(const bool hasData, const bool wifiConnected,
                      const bool lastFetchFailed, const bool ageFresh) {
  return hasData && wifiConnected && !lastFetchFailed && ageFresh;
}

void SnapshotStore::recordSuccess(const LimitsSnapshot &snapshot,
                                  const uint32_t nowMs) {
  _snapshot = snapshot;
  _updatedMs = nowMs;
  _hasData = true;
  _lastFetchFailed = false;
}

void SnapshotStore::recordFailure() { _lastFetchFailed = true; }

bool SnapshotStore::hasData() const { return _hasData; }

bool SnapshotStore::isFresh(const uint32_t nowMs,
                            const uint32_t freshnessMs) const {
  return _hasData && codexlimits::isFresh(_updatedMs, nowMs, freshnessMs);
}

bool SnapshotStore::lastFetchFailed() const { return _lastFetchFailed; }

const LimitsSnapshot &SnapshotStore::snapshot() const { return _snapshot; }

uint32_t SnapshotStore::ageSeconds(const uint32_t nowMs) const {
  if (!_hasData) return 0;
  return static_cast<uint32_t>(nowMs - _updatedMs) / 1000U;
}

}  // namespace codexlimits
