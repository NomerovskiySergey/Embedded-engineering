#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace codexlimits {

constexpr std::size_t kVisibleWindowCount = 2U;

struct LimitWindow {
  char label[12] = {};
  uint8_t usedPercent = 0;
  uint32_t windowMinutes = 0;
  int64_t resetsAt = 0;
};

struct LimitsSnapshot {
  std::array<LimitWindow, kVisibleWindowCount> windows = {};
  uint8_t count = 0;
  bool truncated = false;
  int64_t generatedAt = 0;
};

enum class UsageColor : uint8_t { Green, Amber, Red };

uint8_t remainingPercent(uint32_t usedPercent);
UsageColor usageColor(uint8_t usedPercent);
bool isFresh(uint32_t updatedMs, uint32_t nowMs, uint32_t freshnessMs);
bool displayDataFresh(bool hasData, bool wifiConnected, bool lastFetchFailed,
                      bool ageFresh);

class SnapshotStore {
 public:
  void recordSuccess(const LimitsSnapshot &snapshot, uint32_t nowMs);
  void recordFailure();
  bool hasData() const;
  bool isFresh(uint32_t nowMs, uint32_t freshnessMs) const;
  bool lastFetchFailed() const;
  const LimitsSnapshot &snapshot() const;
  uint32_t ageSeconds(uint32_t nowMs) const;

 private:
  LimitsSnapshot _snapshot = {};
  uint32_t _updatedMs = 0;
  bool _hasData = false;
  bool _lastFetchFailed = false;
};

}  // namespace codexlimits
