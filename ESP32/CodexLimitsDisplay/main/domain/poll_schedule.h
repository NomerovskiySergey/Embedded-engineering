#pragma once

#include <cstdint>

namespace codexlimits {

class PollSchedule {
 public:
  PollSchedule(uint32_t successIntervalMs, uint32_t firstRetryMs,
               uint32_t maximumRetryMs);
  bool isDue(uint32_t nowMs) const;
  void recordSuccess(uint32_t nowMs);
  void recordFailure(uint32_t nowMs);
  uint32_t currentIntervalMs() const;

 private:
  uint32_t _successIntervalMs;
  uint32_t _firstRetryMs;
  uint32_t _maximumRetryMs;
  uint32_t _lastAttemptMs = 0;
  uint32_t _currentIntervalMs = 0;
  bool _attempted = false;
};

}  // namespace codexlimits
