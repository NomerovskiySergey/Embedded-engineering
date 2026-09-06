#include "domain/poll_schedule.h"

namespace alertsiren {

PollSchedule::PollSchedule(const uint32_t successIntervalMs,
                           const uint32_t firstFailureIntervalMs,
                           const uint32_t maximumFailureIntervalMs)
    : _successIntervalMs(successIntervalMs),
      _firstFailureIntervalMs(firstFailureIntervalMs),
      _maximumFailureIntervalMs(maximumFailureIntervalMs),
      _lastAttemptMs(0),
      _currentIntervalMs(0) {}

bool PollSchedule::isDue(const uint32_t nowMs) const {
  return static_cast<uint32_t>(nowMs - _lastAttemptMs) >= _currentIntervalMs;
}

void PollSchedule::recordSuccess(const uint32_t nowMs) {
  _lastAttemptMs = nowMs;
  _currentIntervalMs = _successIntervalMs;
}

void PollSchedule::recordFailure(const uint32_t nowMs) {
  _lastAttemptMs = nowMs;
  if (_currentIntervalMs < _firstFailureIntervalMs ||
      _currentIntervalMs == _successIntervalMs) {
    _currentIntervalMs = _firstFailureIntervalMs;
    return;
  }
  if (_currentIntervalMs >= _maximumFailureIntervalMs / 2U) {
    _currentIntervalMs = _maximumFailureIntervalMs;
  } else {
    _currentIntervalMs *= 2U;
  }
}

uint32_t PollSchedule::currentIntervalMs() const { return _currentIntervalMs; }

}  // namespace alertsiren
