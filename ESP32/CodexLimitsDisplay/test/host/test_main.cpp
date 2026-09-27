#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "domain/limits_model.h"
#include "domain/poll_schedule.h"
#include "providers/limits_parser.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                     \
  do {                                                                       \
    if (!(condition)) {                                                      \
      std::cerr << __func__ << ':' << __LINE__ << ": check failed: "       \
                << #condition << '\n';                                       \
      ++failures;                                                            \
    }                                                                        \
  } while (false)

using codexlimits::LimitsSnapshot;
using codexlimits::UsageColor;

void valid_windows_are_parsed() {
  const auto result = codexlimits::parseLimitsPayload(
      R"({"version":1,"generated_at":1789780000,"windows":[{"label":"5H","used_percent":25,"window_minutes":300,"resets_at":1789783600},{"label":"7D","used_percent":80,"window_minutes":10080,"resets_at":1790384800}],"truncated":false})");
  CHECK(result.valid);
  CHECK(result.snapshot.count == 2U);
  CHECK(std::strcmp(result.snapshot.windows[0].label, "5H") == 0);
  CHECK(result.snapshot.windows[0].usedPercent == 25U);
  CHECK(result.snapshot.windows[0].windowMinutes == 300U);
  CHECK(result.snapshot.windows[0].resetsAt == 1789783600);
  CHECK(std::strcmp(result.snapshot.windows[1].label, "7D") == 0);
  CHECK(!result.snapshot.truncated);
}

void malformed_and_unsafe_payloads_are_rejected() {
  const char *const invalid[] = {
      "", "{", "[]",
      R"({"version":1,"generated_at":1,"windows":[],"truncated":false})",
      R"({"version":1,"generated_at":1,"windows":[{"label":"5H","used_percent":101,"window_minutes":300,"resets_at":2}],"truncated":false})",
      R"({"version":1,"generated_at":1,"windows":[{"label":"5H","used_percent":-1,"window_minutes":300,"resets_at":2}],"truncated":false})",
      R"({"version":1,"generated_at":1,"windows":[{"label":"5H","used_percent":1,"used_percent":2,"window_minutes":300,"resets_at":2}],"truncated":false})",
      R"({"version":1,"generated_at":1,"windows":[{"label":"LABEL-IS-TOO-LONG","used_percent":1,"window_minutes":300,"resets_at":2}],"truncated":false})",
      R"({"version":1,"generated_at":1,"windows":[{"label":"5H","used_percent":1,"window_minutes":0,"resets_at":2}],"truncated":false})"};
  for (const char *payload : invalid) {
    CHECK(!codexlimits::parseLimitsPayload(payload).valid);
  }
  const std::string oversized(4097, 'x');
  CHECK(!codexlimits::parseLimitsPayload(oversized).valid);
}

void more_than_two_windows_are_bounded_and_flagged() {
  const auto result = codexlimits::parseLimitsPayload(
      R"({"version":1,"generated_at":1789780000,"windows":[{"label":"A","used_percent":1,"window_minutes":1,"resets_at":2},{"label":"B","used_percent":2,"window_minutes":2,"resets_at":3},{"label":"C","used_percent":3,"window_minutes":3,"resets_at":4}],"truncated":false})");
  CHECK(result.valid);
  CHECK(result.snapshot.count == 2U);
  CHECK(result.snapshot.truncated);
  CHECK(std::strcmp(result.snapshot.windows[0].label, "A") == 0);
  CHECK(std::strcmp(result.snapshot.windows[1].label, "B") == 0);
}

void remaining_and_color_thresholds_are_stable() {
  CHECK(codexlimits::remainingPercent(0U) == 100U);
  CHECK(codexlimits::remainingPercent(100U) == 0U);
  CHECK(codexlimits::remainingPercent(250U) == 0U);
  CHECK(codexlimits::usageColor(69U) == UsageColor::Green);
  CHECK(codexlimits::usageColor(70U) == UsageColor::Amber);
  CHECK(codexlimits::usageColor(89U) == UsageColor::Amber);
  CHECK(codexlimits::usageColor(90U) == UsageColor::Red);
  CHECK(codexlimits::usageColor(100U) == UsageColor::Red);
}

void freshness_handles_boundaries_and_rollover() {
  CHECK(codexlimits::isFresh(1000U, 300999U, 300000U));
  CHECK(!codexlimits::isFresh(1000U, 301000U, 300000U));
  CHECK(codexlimits::isFresh(UINT32_MAX - 1000U, 1998U, 3000U));
  CHECK(!codexlimits::isFresh(UINT32_MAX - 1000U, 1999U, 3000U));
}

void last_valid_snapshot_is_not_replaced_by_failure() {
  LimitsSnapshot current = {};
  current.count = 1U;
  current.windows[0].usedPercent = 42U;
  codexlimits::SnapshotStore store;
  store.recordSuccess(current, 1000U);
  store.recordFailure();
  CHECK(store.hasData());
  CHECK(store.snapshot().windows[0].usedPercent == 42U);
  CHECK(!store.isFresh(301000U, 300000U));
  CHECK(store.lastFetchFailed());
}

void visible_freshness_requires_live_transport_health() {
  CHECK(codexlimits::displayDataFresh(true, true, false, true));
  CHECK(!codexlimits::displayDataFresh(true, false, false, true));
  CHECK(!codexlimits::displayDataFresh(true, true, true, true));
  CHECK(!codexlimits::displayDataFresh(true, true, false, false));
  CHECK(!codexlimits::displayDataFresh(false, true, false, true));
}

void retry_schedule_is_bounded_and_recovers() {
  codexlimits::PollSchedule schedule(60000U, 5000U, 60000U);
  CHECK(schedule.isDue(0U));
  schedule.recordFailure(0U);
  CHECK(!schedule.isDue(4999U));
  CHECK(schedule.isDue(5000U));
  schedule.recordFailure(5000U);
  CHECK(schedule.currentIntervalMs() == 10000U);
  schedule.recordFailure(15000U);
  CHECK(schedule.currentIntervalMs() == 20000U);
  schedule.recordFailure(35000U);
  schedule.recordFailure(75000U);
  CHECK(schedule.currentIntervalMs() == 60000U);
  schedule.recordSuccess(135000U);
  CHECK(schedule.currentIntervalMs() == 60000U);
  CHECK(!schedule.isDue(194999U));
  CHECK(schedule.isDue(195000U));
}

}  // namespace

int main() {
  valid_windows_are_parsed();
  malformed_and_unsafe_payloads_are_rejected();
  more_than_two_windows_are_bounded_and_flagged();
  remaining_and_color_thresholds_are_stable();
  freshness_handles_boundaries_and_rollover();
  last_valid_snapshot_is_not_replaced_by_failure();
  visible_freshness_requires_live_transport_health();
  retry_schedule_is_bounded_and_recovers();
  if (failures != 0) {
    std::cerr << failures << " test checks failed\n";
    return 1;
  }
  std::cout << "All Codex limits firmware host tests passed\n";
  return 0;
}
