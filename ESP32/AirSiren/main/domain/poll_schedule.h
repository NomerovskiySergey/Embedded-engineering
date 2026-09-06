#pragma once

#include <cstdint>

namespace alertsiren {

class PollSchedule {
 public:
  PollSchedule(uint32_t successIntervalMs, uint32_t firstFailureIntervalMs,
               uint32_t maximumFailureIntervalMs);

  bool isDue(uint32_t nowMs) const;
  void recordSuccess(uint32_t nowMs);
  void recordFailure(uint32_t nowMs);
  uint32_t currentIntervalMs() const;

 private:
  uint32_t _successIntervalMs;
  uint32_t _firstFailureIntervalMs;
  uint32_t _maximumFailureIntervalMs;
  uint32_t _lastAttemptMs;
  uint32_t _currentIntervalMs;
};

}  // namespace alertsiren
