# FEAT-003: Automatic night display dimming

## Status

`done`

## Intent and acceptance criteria

- [ ] From 23:00 through 06:59 Kyiv local time, non-alert screens use 8% backlight instead of the normal 40%.
- [ ] A fresh Alert immediately restores 40% brightness at any hour; clearing at night returns to 8%.
- [ ] From 07:00 through 22:59 every state uses 40% brightness.
- [ ] Invalid/unavailable wall-clock time fails visibly at 40% brightness.
- [ ] Kyiv daylight-saving changes use timezone rules rather than a fixed UTC offset.
- [ ] Wi-Fi, API polling, stale handling, and RGB LED remain active and unchanged; no sleep is introduced.
- [ ] Brightness policy is host-testable and firmware builds for ESP32-C6FH8.

## Scope

- Allowed paths: `ESP32/AirSiren/main/domain/`, `main/platform/display.*`, `main/main.cpp`, `main/CMakeLists.txt`, `test/`, and `README.md`.
- Explicit exclusions: deep/light sleep, API or LED behavior changes, ambient sensor/configuration UI, automatic upload, and unapproved Git actions.

## Explorer proposal

- Evidence:
  - `Display::begin()` fixes GPIO22 LEDC at duty 409/1023 (~40%) and exposes no runtime brightness API.
  - The main loop already has SNTP-backed epoch validation, runs every 100 ms, and redraws only when visible state changes.
  - Computed alert state and RGB LED authority are independent, so brightness can consume state without affecting safety semantics.
- Options and trade-offs:
  - Recommended: scheduled PWM dimming while networking and polling stay active. This targets the display's visible/power-heavy part without alert latency.
  - Always-on 40% is simplest but unnecessarily bright overnight.
  - Deep/light sleep saves more MCU power but adds reconnection latency and conflicts with continuous monitoring.
  - Backlight fully off saves more but hides network/stale failures and lacks a defined wake interaction.
- Recommended decision:
  - Add a pure backlight-policy module mapping local hour, clock validity, and authoritative state to a bounded 10-bit duty.
  - Use duty 409 normally and 82 (~8%) from 23:00 to 07:00 for non-alert states; Alert always uses 409.
  - Configure Kyiv POSIX timezone/DST rules once and use `localtime_r` only after epoch validation.
  - Add a display method that updates LEDC only when the desired duty changes; do not redraw or suspend the display.
- Risks:
  - A fixed UTC offset would mishandle seasonal time; timezone rules are required.
  - Eight percent must be confirmed readable on the physical panel.
  - Before time synchronization, boot stays at 40%, deliberately favoring visibility.

## Human decision

| Gate | Status | Date | Decision maker | Decision/rationale | Requested changes |
| --- | --- | --- | --- | --- | --- |
| Solution | approved |  |  |  |  |
| Test design | approved |  |  |  |  |
| Branch | approved |  |  |  |  |
| Review resolution | approved |  |  |  |  |
| Commit | approved |  |  |  |  |
| Acceptance | approved |  |  |  |  |

## TDD test plan

- Proposed failing tests:
  1. Non-alert states at 23:00 and 06:59 select duty 82.
  2. Boundary hours 22:59 and 07:00 select duty 409.
  3. Alert always selects duty 409, including at night.
  4. Invalid/unavailable wall clock selects duty 409.
  5. Selected duties remain within the 10-bit LEDC range.
  6. Existing alert, threat, freshness, and display-profile tests remain green.
- Verification: `sh test/run_host_tests.sh`, then `/Users/sergejnomerovskij/.platformio/penv/bin/pio run`; physical brightness check requires upload approval.

## Review findings

| Priority | File | Finding | Required resolution |
| --- | --- | --- | --- |
| Required | `main/domain/backlight_policy.cpp` | With `clockValid=true`, invalid hours are inconsistent (`-1` dims while `24` stays bright), contrary to fail-visible behavior. | Validate the range 0–23 and add regression tests for `-1` and `24`, both selecting duty 409. |
| Required | `main/main.cpp` | `ESP_ERROR_CHECK` on a runtime PWM adjustment reboots the entire monitor if LEDC fails, interrupting API and LED behavior. | Do not terminate monitoring on a backlight error; log and retry while keeping the alert loop active. Add a practical seam/test if possible. |

## Verification evidence

| Command | Result | Notes |
| --- | --- | --- |
| Explorer source inspection | pass | Fixed backlight duty, existing valid epoch, 100 ms loop, and separate LED authority confirmed; application/test code unchanged. |
| `harness_git_plan` | pass | Started from clean `main`; user required and Harness created `feature/air-siren-night-mode`. |
| Initial `sh test/run_host_tests.sh` | expected failure | Compilation stopped because approved `domain/backlight_policy.h` did not exist; red phase recorded before production changes. |
| `sh test/run_host_tests.sh` after implementation | pass | 27 host tests passed, including night/day boundaries, Alert override, invalid-clock fallback, and duty bounds. |
| `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | ESP32-C6 build succeeded; RAM 71,936/327,680 bytes, flash 1,190,240/4,194,304 bytes. |
| Agentic Harness plugin update | pass | Source rule and runtime validation now require `feature/` or `bug/`; plugin/skill validation and runtime/isolation smoke tests passed; cachebuster version reinstalled. |
| Independent code review | changes required | 27 tests and build pass, but invalid local-hour handling and fatal runtime PWM error handling require resolution before independent testing. |
| Invalid-hour regression run before fix | expected failure | Both `-1` and `24` selected incorrect brightness instead of safe duty 409; two checks failed before production changes. |
| `sh test/run_host_tests.sh` after review fixes | pass | 27 host tests passed, including invalid-hour fail-bright regressions. |
| `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` after review fixes | pass | ESP32-C6 build succeeded; RAM 71,936 bytes, flash 1,190,262 bytes. |
| Independent re-review | approved | Both required findings resolved; no actionable findings, 27 tests/build and diff check pass. |
| Independent tester | pass with hardware caveat | All automated acceptance criteria pass: 27 host tests, ESP32-C6 build, RAM 71,936 bytes, flash 1,190,262 bytes. Physical 8% readability and live transition remain for human acceptance. |

## Handoffs

| Stage | Status | Evidence | Next action |
| --- | --- | --- | --- |
| Explorer | complete | Scheduled PWM dimming proposed without sleep or polling changes. | Human approves, amends, or rejects the solution. |
| Implementer | complete | Test-first domain policy, Kyiv timezone fail-safe, runtime LEDC adjustment, build integration, and README documentation implemented; 27 tests and firmware build pass. | Independent code review. |
| Code reviewer | changes required | Found two required fail-safe gaps: invalid hour range and reboot-on-backlight-error. | Human approves, amends, or rejects the proposed dispositions before bugfix work. |
| Review-finding resolution | complete | Invalid hours now fail bright; runtime PWM errors are logged once per failure episode and retried every loop without rebooting monitoring. | Independent re-review. |
| Code re-review | approved | Fail-bright validation and nonfatal retry semantics verified; previous behavior remains sound. | Independent acceptance testing. |
| Tester | passed | Verified boundaries, Alert override, fail-bright behavior, Kyiv DST rule, nonfatal PWM retry, unchanged API/RGB behavior, no sleep, documentation, scope, tests, and build. | Human uploads and accepts or requests changes after the physical brightness check. |
| Human acceptance | accepted | User explicitly requested closing FEAT-003 after independent review and testing passed. | Knowledge curator updates durable project context; commit remains a separate approval. |
| Knowledge curator | complete | Current context, architecture, conventions, and feature index updated with the accepted behavior. | Commit the accepted feature only after explicit Commit-gate approval. |

## Knowledge updates

- Current context: FEAT-003 accepted and complete; 27 host tests and ESP32-C6 build pass.
- Decision records: no new ADR required; this refines the existing display-output architecture.
- Architecture/conventions: updated with scheduled fail-bright backlight behavior and nonfatal peripheral-update handling.

<!-- approval: Solution; rationale: User approved Kyiv-scheduled 8% non-alert night dimming, 40% alert override, and continuous Wi-Fi/API operation without sleep. -->

| awaiting_solution_approval | test_design | Solution explicitly approved by user; advancing to detailed TDD design. | review feature document |

| test_design | awaiting_test_approval | Six test behaviors documented: night/day boundaries, alert override, invalid-clock fallback, LEDC bounds, and unchanged existing suite. | review feature document |

<!-- approval: Test design; rationale: User explicitly approved the six test-first brightness-policy behaviors and verification commands. -->

<!-- approval: Branch; rationale: User approved branch creation with required naming change: use feature/ for features and bug/ for bug fixes; selected feature/air-siren-night-mode from main. -->

| awaiting_test_approval | implementing | Solution, test design, and feature/air-siren-night-mode branch explicitly approved; begin test-first implementation. | review feature document |

<!-- approval: Review resolution; rationale: User approved both required findings test-first: invalid local hours fail bright, and runtime PWM errors log/retry without rebooting alert monitoring. -->

| awaiting_review_resolution | implementing | Review resolution explicitly approved; add invalid-hour regression tests before production fixes, then make PWM errors nonfatal and retryable. | review feature document |

<!-- approval: Acceptance; rationale: User explicitly instructed to close the feature after review and independent testing passed. -->

<!-- approval: Acceptance; rationale: Compatibility recording for the user's explicit acceptance ('закривай'); runtime transition currently recognizes approved rather than accepted. -->

| awaiting_acceptance | curating | User explicitly accepted and requested closure; proceed to durable knowledge updates. Commit remains unapproved. | review feature document |

| curating | done | Human accepted closure; durable current context, architecture, conventions, and feature index updated. Commit remains pending explicit approval. | review feature document |

<!-- approval: Commit; rationale: User explicitly approved exact message 'feat(airsiren): add automatic night dimming', the 13-file scope, and reported validation summary. -->
