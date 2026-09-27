#include "domain/poll_schedule.h"

#include <algorithm>

namespace codexlimits {

PollSchedule::PollSchedule(const uint32_t successIntervalMs,
                           const uint32_t firstRetryMs,
                           const uint32_t maximumRetryMs)
    : _successIntervalMs(successIntervalMs),
      _firstRetryMs(firstRetryMs),
      _maximumRetryMs(maximumRetryMs),
      _currentIntervalMs(successIntervalMs) {}

bool PollSchedule::isDue(const uint32_t nowMs) const {
  return !_attempted ||
         static_cast<uint32_t>(nowMs - _lastAttemptMs) >= _currentIntervalMs;
}

void PollSchedule::recordSuccess(const uint32_t nowMs) {
  _lastAttemptMs = nowMs;
  _currentIntervalMs = _successIntervalMs;
  _attempted = true;
}

void PollSchedule::recordFailure(const uint32_t nowMs) {
  _lastAttemptMs = nowMs;
  if (!_attempted || _currentIntervalMs == _successIntervalMs) {
    _currentIntervalMs = _firstRetryMs;
  } else {
    _currentIntervalMs =
        std::min(_currentIntervalMs * 2U, _maximumRetryMs);
  }
  _attempted = true;
}

uint32_t PollSchedule::currentIntervalMs() const { return _currentIntervalMs; }

}  // namespace codexlimits
