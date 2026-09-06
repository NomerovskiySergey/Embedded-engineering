# FEAT-002: Current threat type on AirSiren display

## Status

`done`

## Intent and acceptance criteria

- [x] During an active alert, the display shows the current reported threat type relevant to Dnipro/Dnipropetrovsk oblast.
- [x] Supported labels distinguish missile, guided bomb, strike drone, reconnaissance drone, general warning, other, and unknown/multiple threats.
- [x] Threat information never changes the authoritative red/green alert decision.
- [x] Missing, malformed, stale, or unavailable threat data displays `THREAT: UNKNOWN` during an alert and never produces a false clear.
- [x] Only active, unexpired events relevant to Dnipropetrovsk oblast or explicitly targeting Dnipro are considered.
- [x] Multiple simultaneous relevant types are represented without hiding the higher-urgency type.
- [x] Parsing and relevance/priority behavior have host-runnable tests; firmware still builds for ESP32-C6FH8.
- [x] README explains that threat types are AI-processed open-source reports from an undocumented live-map endpoint, not official radar data.

## Scope

- Allowed paths:
  - `.agent-harness/`
  - `ESP32/AirSiren/`
- Explicit exclusions:
  - changing the official alert/clear decision source
  - displaying trajectories or exact coordinates
  - treating 24-hour aggregate statistics as a current threat
  - disabling TLS verification

## Explorer proposal

- Evidence:
  - The documented regional alert endpoint exposes only active/clear and district state; it has no current threat type.
  - The documented statistics endpoint exposes aggregate `threats` counts over 24h/7d/30d, which cannot identify the cause of a current alert.
  - The public live map loads `/api/events`. A live response inspected on 2026-09-06 exposes `type`, `type_label`, `subtype`, `oblast`, `location_name`, `target_name`, `is_active`, `reported_at`, `expires_at`, and `age_minutes`.
  - A live example included `type:m`, `subtype:cruise`, oblast `Дніпропетровська`, and target `Дніпро`; other observed values include `type:d` and `subtype:kab`.
  - Tryvoha states that live threat markers are produced by automated analysis of public Telegram/Ukrainian Air Force messages and are not radar tracks or an official warning source.
- Options and trade-offs:
  - Use `/api/events`: current and sufficiently structured, no key, but undocumented and therefore schema/availability may change.
  - Use `/api/v1/stats/{slug}`: documented and stable, but aggregate/history-only and unsafe to label as current.
  - Switch to alerts.in.ua: documented alert categories but requires a token and does not itself distinguish missile versus drone for ordinary `air_raid` alerts.
- Recommended decision:
  - Keep `/api/v1/alerts/dnipropetrovska` as the sole safety-state source.
  - Poll `/api/events` as a secondary informational feed on the same 30-second cadence and over verified TLS.
  - Parse the event array fail-closed with a bounded response size. Consider only `is_active:true` entries whose oblast is Dnipropetrovsk or whose target is Dnipro.
  - Map `m+kab` to `KAB`, other `m` to `MISSILE`, `d` to `DRONE`, `rd` to `RECON DRONE`, `a` to `WARNING`, and unknown values to `OTHER`.
  - If several types apply, show `MULTIPLE`; if no trustworthy current event exists during an official alert, show `THREAT: UNKNOWN`.
  - Show no threat label in `ALL CLEAR`; show `THREAT: UNKNOWN` only on red alert screens. Threat feed failure must not alter the LED or alert state.
- Risks:
  - `/api/events` is not part of the documented developer API and can change without notice.
  - Region-level relevance can over-report a threat far from Dnipro; explicit Dnipro targeting is stronger evidence but not always present.
  - Automated source analysis may misclassify or lag threats.
  - The events payload is larger than the existing 4 KiB alert response buffer; implementation must use a separately bounded buffer or a bounded streaming parser without returning large storage to the task stack.

## Human decision

| Gate | Status | Date | Decision maker | Decision/rationale | Requested changes |
| --- | --- | --- | --- | --- | --- |
| Solution | approved | 2026-09-06 | user | Approved the secondary `/api/events` feed, conservative relevance rules, informational-only UI, and fail-closed behavior without changes. | None. |
| Test design | approved | 2026-09-06 | user | Approved all nine test behaviors and the test-first implementation order. | None. |
| Branch | approved | 2026-09-06 | user | Approved continuing on the existing `codex/air-siren` branch. | None. |
| Review resolution | approved | 2026-09-06 | user | Approved all review rounds test-first, including strict calendar validation for `expires_at`. | None. |
| Unicode bugfix review resolution | approved | 2026-09-06 | user | Approved replacing exact escaped-location matching with bounded JSON Unicode decoding and adding uppercase/mixed regression tests. | None. |
| Commit | approved | 2026-09-06 | user | User explicitly requested creating the Git commit after physical acceptance. | None. |
| Acceptance | approved | 2026-09-06 | user | User flashed the corrected firmware and confirmed that threat classification now works on the physical board with live API data. | None. |

## TDD test plan

- Test boundary:
  - Add an ESP-independent `ThreatType` model and `parseTryvohaEvents()` parser.
  - Feed short inline JSON fixtures; tests perform no network requests and contain no credentials.
  - Transport remains an ESP-IDF integration layer; display formatting receives a parsed enum rather than raw JSON.
- Failing tests to add before production code:
  1. `active_cruise_missile_for_oblast_is_missile`: active `m/cruise` in Dnipropetrovsk maps to `Missile`.
  2. `active_kab_for_oblast_is_kab`: active `m/kab` maps to `GuidedBomb`.
  3. `drone_types_remain_distinct`: `d` maps to strike drone and `rd` to reconnaissance drone.
  4. `explicit_dnipro_target_is_relevant`: an active event explicitly targeting Dnipro is retained even when the reported current oblast differs.
  5. `inactive_expired_and_unrelated_events_are_ignored`: none can create a current threat label.
  6. `multiple_relevant_types_are_multiple`: two distinct relevant active types map to `Multiple`; duplicates remain one type.
  7. `malformed_oversized_and_unknown_payloads_fail_closed`: result is invalid/unknown without affecting alert state.
  8. `threat_label_only_appears_during_official_alert`: clear/startup/stale screens never present a current threat as authoritative.
  9. `threat_data_expires_independently`: stale threat data becomes unknown while the official alert may remain red.
- TDD order:
  - Add model/parser tests and record their compile failure.
  - Implement the minimum pure model/parser until that group passes.
  - Add UI gating/freshness tests and record failure, then implement them.
  - Integrate a second bounded HTTPS fetch, preserving the official alert path and static/heap storage discipline established after the stack-overflow defect.
  - Refactor only while the full host suite remains green.
- Verification commands:
  - `sh test/run_host_tests.sh`
  - `/Users/sergejnomerovskij/.platformio/penv/bin/pio run`
  - manual device check with controlled parser fixtures and live feed after upload approval

## Review findings

| Priority | File | Finding | Required resolution |
| --- | --- | --- | --- |
| Required | `main/providers/tryvoha_events_parser.cpp` | Bugfix recognizes only the currently observed lowercase, fully escaped spelling. Valid JSON may use uppercase hex digits or mix raw UTF-8 with escapes and would still yield `UNKNOWN`. | Decode JSON string escapes (including `\\uXXXX`, both hex cases) before location comparison and add uppercase/mixed regression fixtures. |

## Verification evidence

| Command | Result | Notes |
| --- | --- | --- |
| Live `/api/events` inspection after hardware report | defect reproduced | API returned an active Dnipropetrovsk `type:d` event, but location text was JSON `\\u` escaped; the parser compares only decoded UTF-8 and therefore reports `UNKNOWN`. |
| `sh test/run_host_tests.sh` with escaped-location regression fixtures | expected failure | Both escaped Dnipropetrovsk oblast and escaped Dnipro target were ignored, reproducing the device symptom before production changes. |
| `sh test/run_host_tests.sh` after minimal live-payload fix | pass | 25 host tests passed. |
| `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` after minimal live-payload fix | pass | RAM 71,808/327,680 bytes; flash 1,181,442/4,194,304 bytes. |
| Independent bugfix tester | pass with hardware caveat | 25 tests and target build pass; escaped oblast and target fixtures classify correctly, but upload/live transition remains a human check. |
| Independent bugfix review | changes required | Exact escaped-string matching is not invariant to valid uppercase hex or mixed UTF-8/escape JSON serializations; proper bounded decoding is required. |
| Unicode-variant regression run before decoder | expected failure | Uppercase-hex and mixed UTF-8/escape fixtures both remained `UNKNOWN`, establishing the second red phase. |
| `sh test/run_host_tests.sh` after bounded Unicode decoder | pass | 26 host tests passed, covering raw UTF-8, observed lowercase escapes, uppercase hex, and mixed representation. |
| `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` after bounded Unicode decoder | pass | ESP32-C6 build succeeded; RAM 71,808/327,680 bytes, flash 1,182,014/4,194,304 bytes. |
| Final independent Unicode re-review | approved | No findings; bounded allocation-free decoding, surrogate handling, exact UTF-8 comparison, and existing safety boundaries verified. |
| Final independent Unicode re-test | pass with hardware caveat | 26 tests and target build pass; all tested location encodings classify and threat data remains separate from official status and LED. |
| Public live endpoint inspection | pass | `/api/events` returned current typed and expiring event objects, including Dnipropetrovsk/Dnipro examples. |
| Initial threat test compile | expected failure | `tryvoha_events_parser.cpp` did not exist; red phase recorded before implementation. |
| Review-regression `sh test/run_host_tests.sh` before fixes | expected failure | Three checks failed: malformed boolean, expired event, and unknown-type fallback. |
| `sh test/run_host_tests.sh` after review fixes | pass | 22 host tests passed, including TTL, bounds, duplicates, labels, future types, and malformed booleans. |
| `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` after review fixes | pass | RAM 71,808/327,680 bytes; flash 1,180,144/4,194,304 bytes. |
| Second review-regression host run before fixes | expected failure | Compilation failed because the parser had no explicit current-time input; duplicate-field behavior was also newly asserted. |
| `sh test/run_host_tests.sh` after second review fixes | pass | 23 host tests passed with server `expires_at` comparison and duplicate critical-field rejection. |
| Final `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | RAM 71,808/327,680 bytes; flash 1,180,932/4,194,304 bytes. |
| Independent re-review `sh test/run_host_tests.sh` | pass | 22 host tests pass; the expiry-contract and duplicate-field gaps above remain. |
| Independent re-review `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | ESP32-C6 build succeeds; RAM 71,808/327,680 bytes, flash 1,180,144/4,194,304 bytes. |
| Third independent re-review `sh test/run_host_tests.sh` | pass | 23 host tests pass; explicit epoch expiry and duplicate critical-field rejection are covered, but invalid calendar dates are not. |
| Third independent re-review `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | ESP32-C6 build succeeds; RAM 71,808/327,680 bytes, flash 1,180,932/4,194,304 bytes. |
| Calendar regression host run before fix | expected failure | Impossible February 31 and April 31 fixtures were incorrectly accepted; valid leap day passed. |
| `sh test/run_host_tests.sh` after calendar fix | pass | 24 host tests passed with month-length and Gregorian leap-year validation. |
| `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` after calendar fix | pass | RAM 71,808/327,680 bytes; flash 1,181,212/4,194,304 bytes. |
| Final independent review `sh test/run_host_tests.sh` | pass | 24 host tests pass, including impossible month-day rejection and valid Gregorian leap-day handling. |
| Final independent review `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | ESP32-C6 target build succeeds; RAM 71,808/327,680 bytes, flash 1,181,212/4,194,304 bytes. |
| Independent tester `sh test/run_host_tests.sh` | pass | 24/24 host tests passed, covering classification, Dnipro/oblast relevance, inactivity and expiry, duplicate types, multiple types, malformed and oversized payloads, strict booleans, duplicate critical fields, calendar validity, stable labels, and official-alert/freshness UI gating. |
| Independent tester `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | ESP32-C6FH8 target build succeeded; RAM 71,808/327,680 bytes (21.9%), flash 1,181,212/4,194,304 bytes (28.2%). |
| Independent tester integration inspection | pass with hardware caveat | Verified separate verified-TLS requests, a bounded 32 KiB response buffer, independent official alert state and threat freshness, and alert-only display rendering. Tryvoha's public map/API material still identifies the expected threat codes and UTC/TTL semantics. No firmware upload or controlled live-alert hardware exercise was performed in this test stage. |

## Handoffs

| Stage | Status | Evidence | Next action |
| --- | --- | --- | --- |
| Explorer | complete | Documented API, live-map implementation, live payload, alternatives, safety boundary, and payload-size risk inspected. | Human approves or amends the solution. |
| Solution gate | accepted | User approved the Explorer proposal on 2026-09-06 without changes. | Human approves or amends the detailed TDD plan and branch choice. |
| Test-design gate | accepted | User approved the detailed TDD plan and current branch on 2026-09-06. | Add failing tests before production code. |
| Implementer | complete | Red phases recorded; threat model/parser, bounded HTTPS feed, independent freshness, UI label, and documentation implemented; 22 tests and target build pass. | Repeat independent code review. |
| Code review | changes required | Three required findings: expiry is ignored, unknown valid types do not map to `OTHER`, and approved parser/UI boundary coverage plus red-phase evidence is incomplete. | Resolve findings test-first and return for independent review before testing. |
| Code re-review | changes required | Unknown-type mapping, labels, payload bounds, duplicate same-type handling, and TDD records are now covered, but actual `expires_at` remains unused and duplicate JSON fields remain accepted. | Resolve the two remaining findings test-first and repeat independent review. |
| Second code re-review | changes required | Actual `expires_at` is now compared against an explicit epoch and critical duplicate fields are rejected. One malformed-calendar-date defect remains in timestamp validation. | Add strict calendar validation test-first and repeat independent review. |
| Final code re-review | approved | Calendar validation now enforces month lengths and Gregorian leap years; all previous required findings remain resolved; 24 host tests and target build pass. | Proceed to independent testing against the acceptance criteria. |
| Tester | passed | All eight acceptance criteria are covered by host tests, target-build evidence, source-level integration inspection, and README inspection. Hardware rendering and a real transition during a live alert remain for human acceptance because no upload or controllable live threat event was available. | Human performs/accepts the device check and approves or rejects the feature. |
| Unicode bugfix | passed review and testing | The live-payload defect was reproduced test-first, then fixed with bounded JSON string decoding; 26 tests and target build pass. | Upload to the board and verify that a live relevant event no longer remains `UNKNOWN`. |
| Human acceptance | accepted | User flashed the build and reported “тепер працює”, confirming the live Unicode-location fix on the physical ESP32-C6 display. | Curate durable project knowledge; leave Git commit pending explicit approval. |
| Knowledge curator | complete | Architecture, conventions, current context, decision record, and feature index updated from the accepted implementation. | Next session may commit the accepted project only after explicit user approval. |

## Knowledge updates

- Current context: FEAT-002 and its Unicode live-payload bugfix passed independent review/testing and were accepted on physical hardware with live API data; 26 host tests and the ESP32-C6 target build pass.
- Decision records: `ADR-001-secondary-live-threat-feed.md` records the informational threat-feed boundary.
- Architecture/conventions: updated with the accepted dual-feed design and bounded JSON Unicode parsing rule.
