#include <cstdint>
#include <iostream>
#include <string>

#include "domain/alert_status.h"
#include "domain/poll_schedule.h"
#include "providers/tryvoha_parser.h"
#include "providers/tryvoha_events_parser.h"
#include "platform/waveshare_display_profile.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      std::cerr << __func__ << ':' << __LINE__ << ": check failed: "          \
                << #condition << '\n';                                          \
      ++failures;                                                               \
    }                                                                           \
  } while (false)

using alertsiren::AlertState;
using alertsiren::ThreatType;

void clear_response_is_clear() {
  const auto result = alertsiren::parseTryvohaResponse(
      R"({"slug":"dnipropetrovska","type":"state","active":false,"started_at":null,"districts_active":[],"active_anywhere":false})");
  CHECK(result.valid);
  CHECK(result.state == AlertState::Clear);
}

void oblast_alert_is_alert() {
  const auto result = alertsiren::parseTryvohaResponse(
      R"({"slug":"dnipropetrovska","type":"state","active":true,"started_at":"2026-09-06T09:00:00Z","districts_active":[],"active_anywhere":true})");
  CHECK(result.valid);
  CHECK(result.state == AlertState::Alert);
}

void dniprovskyi_district_is_alert() {
  const auto result = alertsiren::parseTryvohaResponse(
      R"({"slug":"dnipropetrovska","type":"state","active":false,"started_at":null,"districts_active":[{"slug":"dniprovskii-raion","name_uk":"Дніпровський район","started_at":"2026-09-06T09:00:00Z"}],"active_anywhere":true})");
  CHECK(result.valid);
  CHECK(result.state == AlertState::Alert);
}

void unrelated_district_is_not_target_alert() {
  const auto result = alertsiren::parseTryvohaResponse(
      R"({"slug":"dnipropetrovska","type":"state","active":false,"started_at":null,"districts_active":[{"slug":"kryvorizkii-raion","name_uk":"Криворізький район","started_at":"2026-09-06T09:00:00Z"}],"active_anywhere":true})");
  CHECK(result.valid);
  CHECK(result.state == AlertState::Clear);
}

void provider_partial_without_target_detail_is_not_clear() {
  const auto result = alertsiren::parseTryvohaResponse(
      R"({"slug":"dnipropetrovska","type":"state","active":false,"started_at":null,"active_anywhere":true})");
  CHECK(!result.valid);
  CHECK(result.state == AlertState::Unknown);
}

void bad_payloads_fail_closed() {
  const std::string oversized(4097, 'x');
  const char *const invalidPayloads[] = {
      "{", "", "[]",
      R"({"active":false,"districts_active":[],"active_anywhere":false})",
      R"({"slug":"dnipropetrovska","active":"false","districts_active":[],"active_anywhere":false})",
      R"({"slug":"dnipropetrovska","active":true,"districts_active":[],"active_anywhere":false})"};

  for (const char *payload : invalidPayloads) {
    const auto result = alertsiren::parseTryvohaResponse(payload);
    CHECK(!result.valid);
    CHECK(result.state == AlertState::Unknown);
  }
  const auto oversizedResult = alertsiren::parseTryvohaResponse(oversized);
  CHECK(!oversizedResult.valid);
  CHECK(oversizedResult.state == AlertState::Unknown);
}

void status_maps_to_safe_outputs() {
  const auto alert = alertsiren::outputsFor(AlertState::Alert);
  CHECK(alert.redLed && !alert.greenLed);
  CHECK(alert.theme == alertsiren::UiTheme::Red);

  const auto clear = alertsiren::outputsFor(AlertState::Clear);
  CHECK(!clear.redLed && clear.greenLed);
  CHECK(clear.theme == alertsiren::UiTheme::Green);

  for (const AlertState state : {AlertState::Startup, AlertState::Unknown,
                                 AlertState::Stale}) {
    const auto output = alertsiren::outputsFor(state);
    CHECK(!output.redLed && !output.greenLed);
    CHECK(output.theme == alertsiren::UiTheme::Amber);
  }
}

void clear_result_expires_to_stale() {
  CHECK(alertsiren::withFreshness(AlertState::Clear, 1000U, 60999U, 60000U) ==
        AlertState::Clear);
  CHECK(alertsiren::withFreshness(AlertState::Clear, 1000U, 61000U, 60000U) ==
        AlertState::Stale);
  CHECK(!alertsiren::outputsFor(AlertState::Stale).greenLed);
}

void alert_result_expires_to_stale_without_becoming_clear() {
  CHECK(alertsiren::withFreshness(AlertState::Alert, 1000U, 61000U, 60000U) ==
        AlertState::Stale);
}

void successful_poll_waits_30_seconds() {
  alertsiren::PollSchedule schedule(30000U, 5000U, 60000U);
  schedule.recordSuccess(100U);
  CHECK(!schedule.isDue(30099U));
  CHECK(schedule.isDue(30100U));
}

void failures_back_off_with_cap() {
  alertsiren::PollSchedule schedule(30000U, 5000U, 20000U);
  schedule.recordFailure(100U);
  CHECK(schedule.currentIntervalMs() == 5000U);
  schedule.recordFailure(5100U);
  CHECK(schedule.currentIntervalMs() == 10000U);
  schedule.recordFailure(15100U);
  CHECK(schedule.currentIntervalMs() == 20000U);
  schedule.recordFailure(35100U);
  CHECK(schedule.currentIntervalMs() == 20000U);
}

void scheduler_handles_tick_rollover() {
  alertsiren::PollSchedule schedule(30000U, 5000U, 60000U);
  schedule.recordSuccess(UINT32_MAX - 10000U);
  CHECK(!schedule.isDue(19998U));
  CHECK(schedule.isDue(19999U));
}

void success_resets_failure_backoff() {
  alertsiren::PollSchedule schedule(30000U, 5000U, 60000U);
  schedule.recordFailure(0U);
  schedule.recordFailure(5000U);
  CHECK(schedule.currentIntervalMs() == 10000U);
  schedule.recordSuccess(15000U);
  CHECK(schedule.currentIntervalMs() == 30000U);
}

void disconnected_clear_is_immediately_stale() {
  CHECK(alertsiren::effectiveState(AlertState::Clear, false) ==
        AlertState::Stale);
  CHECK(alertsiren::effectiveState(AlertState::Alert, false) ==
        AlertState::Stale);
  CHECK(alertsiren::effectiveState(AlertState::Clear, true) ==
        AlertState::Clear);
}

void malformed_boolean_tokens_fail_closed() {
  const auto result = alertsiren::parseTryvohaResponse(
      R"({"slug":"dnipropetrovska","active":falseeeee,"districts_active":[],"active_anywhere":false})");
  CHECK(!result.valid);
  CHECK(result.state == AlertState::Unknown);
}

void duplicate_status_fields_fail_closed() {
  const auto result = alertsiren::parseTryvohaResponse(
      R"({"slug":"dnipropetrovska","active":false,"active":true,"districts_active":[],"active_anywhere":true})");
  CHECK(!result.valid);
  CHECK(result.state == AlertState::Unknown);
}

void waveshare_lcd_uses_vendor_profile() {
  const auto &profile = alertsiren::waveshareDisplayProfile();
  CHECK(profile.pixelClockHz == 12000000U);
  CHECK(profile.xGap == 34);
  CHECK(profile.yGap == 0);
  CHECK(profile.mirrorX);
  CHECK(!profile.mirrorY);
  CHECK(profile.bgrOrder);
  CHECK(profile.commandCount >= 12U);
  CHECK(profile.commands[0].command == 0x11U);  // Sleep out.
  CHECK(profile.commands[profile.commandCount - 1U].command == 0x29U);
}

void current_threat_events_are_classified_safely() {
  auto result = alertsiren::parseTryvohaEvents(
      R"([{"type":"m","subtype":"cruise","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":2}])");
  CHECK(result.valid && result.type == ThreatType::Missile);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"m","subtype":"kab","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":2}])");
  CHECK(result.valid && result.type == ThreatType::GuidedBomb);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":2}])");
  CHECK(result.valid && result.type == ThreatType::StrikeDrone);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"rd","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":2}])");
  CHECK(result.valid && result.type == ThreatType::ReconDrone);
}

void threat_relevance_and_multiple_types_fail_closed() {
  auto result = alertsiren::parseTryvohaEvents(
      R"([{"type":"m","oblast":"Полтавська","target_name":"Дніпро","is_active":true,"age_minutes":2}])");
  CHECK(result.valid && result.type == ThreatType::Missile);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","oblast":"Одеська","target_name":null,"is_active":true,"age_minutes":2},{"type":"m","oblast":"Дніпропетровська","target_name":null,"is_active":false,"age_minutes":2}])");
  CHECK(result.valid && result.type == ThreatType::Unknown);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":2},{"type":"m","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":2}])");
  CHECK(result.valid && result.type == ThreatType::Multiple);

  CHECK(!alertsiren::parseTryvohaEvents("{").valid);
  CHECK(!alertsiren::parseTryvohaEvents(
      R"([{"type":"m","oblast":"Дніпропетровська","target_name":null,"is_active":trueeeee,"age_minutes":2}])").valid);
}

void threat_expiry_unknown_types_and_bounds_are_safe() {
  auto result = alertsiren::parseTryvohaEvents(
      R"([{"type":"m","subtype":"cruise","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":200,"expires_at":"2026-09-06T12:49:04Z"}])",
      2000000000);
  CHECK(result.valid && result.type == ThreatType::Unknown);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"future-code","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":1}])");
  CHECK(result.valid && result.type == ThreatType::Other);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"a","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":1}])");
  CHECK(result.valid && result.type == ThreatType::Warning);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"u","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":1}])");
  CHECK(result.valid && result.type == ThreatType::Other);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":1},{"type":"d","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":2}])");
  CHECK(result.valid && result.type == ThreatType::StrikeDrone);

  const std::string oversized(32769, 'x');
  CHECK(!alertsiren::parseTryvohaEvents(oversized).valid);
}

void every_threat_type_has_a_stable_label() {
  CHECK(std::string(alertsiren::threatLabel(ThreatType::None)).empty());
  CHECK(std::string(alertsiren::threatLabel(ThreatType::Unknown)) == "UNKNOWN");
  CHECK(std::string(alertsiren::threatLabel(ThreatType::Missile)) == "MISSILE");
  CHECK(std::string(alertsiren::threatLabel(ThreatType::GuidedBomb)) == "KAB");
  CHECK(std::string(alertsiren::threatLabel(ThreatType::StrikeDrone)) == "DRONE");
  CHECK(std::string(alertsiren::threatLabel(ThreatType::ReconDrone)) == "RECON DRONE");
  CHECK(std::string(alertsiren::threatLabel(ThreatType::Warning)) == "WARNING");
  CHECK(std::string(alertsiren::threatLabel(ThreatType::Other)) == "OTHER");
  CHECK(std::string(alertsiren::threatLabel(ThreatType::Multiple)) == "MULTIPLE");
}

void event_expiry_uses_server_timestamp_and_duplicates_are_rejected() {
  constexpr auto payload =
      R"([{"type":"m","subtype":"cruise","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":1,"expires_at":"2026-09-06T12:49:04Z"}])";
  CHECK(alertsiren::parseTryvohaEvents(payload, 1700000000).type ==
        ThreatType::Missile);
  CHECK(alertsiren::parseTryvohaEvents(payload, 2000000000).type ==
        ThreatType::Unknown);

  const auto duplicate = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","type":"m","oblast":"Дніпропетровська","target_name":null,"is_active":true,"age_minutes":1,"expires_at":"2033-05-18T03:33:20Z"}])",
      1700000000);
  CHECK(!duplicate.valid);
}

void event_expiry_rejects_impossible_dates_and_accepts_leap_day() {
  const auto invalidFebruary = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","oblast":"Дніпропетровська","target_name":null,"is_active":true,"expires_at":"2026-02-31T12:00:00Z"}])",
      1700000000);
  CHECK(!invalidFebruary.valid);

  const auto invalidApril = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","oblast":"Дніпропетровська","target_name":null,"is_active":true,"expires_at":"2026-04-31T12:00:00Z"}])",
      1700000000);
  CHECK(!invalidApril.valid);

  const auto leapDay = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","oblast":"Дніпропетровська","target_name":null,"is_active":true,"expires_at":"2028-02-29T12:00:00Z"}])",
      1700000000);
  CHECK(leapDay.valid && leapDay.type == ThreatType::StrikeDrone);
}

void escaped_unicode_locations_are_relevant() {
  auto result = alertsiren::parseTryvohaEvents(
      R"([{"type":"d","oblast":"\u0414\u043d\u0456\u043f\u0440\u043e\u043f\u0435\u0442\u0440\u043e\u0432\u0441\u044c\u043a\u0430","target_name":null,"is_active":true,"expires_at":"2033-05-18T03:33:20.000000Z"}])",
      1700000000);
  CHECK(result.valid && result.type == ThreatType::StrikeDrone);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"m","oblast":"\u041f\u043e\u043b\u0442\u0430\u0432\u0441\u044c\u043a\u0430","target_name":"\u0414\u043d\u0456\u043f\u0440\u043e","is_active":true,"expires_at":"2033-05-18T03:33:20.000000Z"}])",
      1700000000);
  CHECK(result.valid && result.type == ThreatType::Missile);
}

void unicode_location_matching_accepts_valid_json_variants() {
  auto result = alertsiren::parseTryvohaEvents(
      R"([{"type":"rd","oblast":"\u0414\u043D\u0456\u043F\u0440\u043E\u043F\u0435\u0442\u0440\u043E\u0432\u0441\u044C\u043A\u0430","target_name":null,"is_active":true,"expires_at":"2033-05-18T03:33:20Z"}])",
      1700000000);
  CHECK(result.valid && result.type == ThreatType::ReconDrone);

  result = alertsiren::parseTryvohaEvents(
      R"([{"type":"m","oblast":"Дніпро\u043f\u0435\u0442\u0440\u043e\u0432\u0441\u044c\u043a\u0430","target_name":null,"is_active":true,"expires_at":"2033-05-18T03:33:20Z"}])",
      1700000000);
  CHECK(result.valid && result.type == ThreatType::Missile);
}

void threat_visibility_requires_fresh_official_alert() {
  CHECK(alertsiren::visibleThreat(AlertState::Alert, ThreatType::Missile,
                                  1000U, 31000U, 60000U) ==
        ThreatType::Missile);
  CHECK(alertsiren::visibleThreat(AlertState::Alert, ThreatType::Missile,
                                  1000U, 61000U, 60000U) ==
        ThreatType::Unknown);
  for (const auto state : {AlertState::Clear, AlertState::Startup,
                           AlertState::Stale}) {
    CHECK(alertsiren::visibleThreat(state, ThreatType::Missile, 1000U, 2000U,
                                    60000U) == ThreatType::None);
  }
}

}  // namespace

int main() {
  clear_response_is_clear();
  oblast_alert_is_alert();
  dniprovskyi_district_is_alert();
  unrelated_district_is_not_target_alert();
  provider_partial_without_target_detail_is_not_clear();
  bad_payloads_fail_closed();
  status_maps_to_safe_outputs();
  clear_result_expires_to_stale();
  alert_result_expires_to_stale_without_becoming_clear();
  successful_poll_waits_30_seconds();
  failures_back_off_with_cap();
  scheduler_handles_tick_rollover();
  success_resets_failure_backoff();
  disconnected_clear_is_immediately_stale();
  malformed_boolean_tokens_fail_closed();
  duplicate_status_fields_fail_closed();
  waveshare_lcd_uses_vendor_profile();
  current_threat_events_are_classified_safely();
  threat_relevance_and_multiple_types_fail_closed();
  threat_expiry_unknown_types_and_bounds_are_safe();
  every_threat_type_has_a_stable_label();
  event_expiry_uses_server_timestamp_and_duplicates_are_rejected();
  event_expiry_rejects_impossible_dates_and_accepts_leap_day();
  escaped_unicode_locations_are_relevant();
  unicode_location_matching_accepts_valid_json_variants();
  threat_visibility_requires_fresh_official_alert();

  if (failures != 0) {
    std::cerr << failures << " test check(s) failed\n";
    return 1;
  }
  std::cout << "26 host tests passed\n";
  return 0;
}
