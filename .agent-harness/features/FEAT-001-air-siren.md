# FEAT-001: AirSiren display and alert indicator

## Status

`implementing`

## Intent and acceptance criteria

- [ ] A self-contained PlatformIO/ESP-IDF project exists at `ESP32/AirSiren/` for the ESP32-C6FH8 board with 8 MB flash.
- [ ] The ST7789 172 x 320 LCD initializes using the documented board pins and a backlight level no greater than 50%.
- [ ] The device connects to a configured 2.4 GHz Wi-Fi network without committing credentials.
- [ ] Firmware polls the keyless Tryvoha.online regional endpoint over HTTPS no more frequently than once per 30 seconds.
- [ ] The configured location covers Dnipro through Dnipropetrovsk oblast and Dniprovskyi raion alert state.
- [ ] A current active or partial/raion alert produces an unmistakable red screen and red onboard RGB LED.
- [ ] A fresh, explicitly confirmed clear response produces a green screen and green onboard RGB LED.
- [ ] Startup, disconnected, malformed-response, HTTP/TLS failure, and stale-data states are visually distinct and never appear green.
- [ ] The screen shows the location, state, last successful update age, Wi-Fi state, and compact `Дані: tryvoha.online` attribution.
- [ ] Network reconnection and polling use bounded waits/backoff and do not block the display/state-machine task indefinitely.
- [ ] Provider parsing and alert-state transitions have host-runnable automated tests, and the target firmware builds with `pio run`.
- [ ] README documents configuration, build/upload, pinout, provider limitations, and that the device does not replace official alerts or sirens.

## Scope

- Allowed paths:
  - `.agent-harness/`
  - `ESP32/AirSiren/`
- Explicit exclusions:
  - `ESP32/LedBlinking/`
  - `ArduinoUNO/`
  - `Nanit/`
  - flashing hardware, external deployment, or sending API requests during automated unit tests

## Explorer proposal

- Evidence:
  - The connected chip was previously verified by the sibling project as ESP32-C6FH8 with 8 MB flash.
  - The sibling PlatformIO project uses `espressif32@7.0.1`, ESP-IDF, and the `esp32-c6-devkitc-1` board profile.
  - Waveshare documents an ST7789 LCD at 172 x 320: MOSI GPIO6, SCLK GPIO7, CS GPIO14, DC GPIO15, reset GPIO21, backlight GPIO22; RGB LED is GPIO8.
  - Waveshare warns that LCD brightness should remain at or below 50%.
  - Tryvoha.online documents a keyless regional endpoint, a 30–60 second cache, and no benefit from polling more frequently than every 30 seconds.
  - Tryvoha.online distinguishes direct oblast alert state from `districts_active` and `active_anywhere`, which is required because alerts are commonly issued at district level.
  - NEPTUN offers a keyless alerts endpoint and is suitable as a later provider, but its response is broader and attribution is mandatory.
- Options and trade-offs:
  - Official Ukraine Alarm API: authoritative source path but requires a requested API key, so it blocks immediate development and adds secret management.
  - alerts.in.ua IoT API: compact and designed for hardware, but still requires a token.
  - Tryvoha.online: simplest keyless response for one region and district; independent service with no availability guarantee.
  - NEPTUN: keyless and richer, but parsing/traffic/UI semantics are unnecessarily complex for the first release.
- Recommended decision:
  - Build an ESP-IDF C++ application while retaining the proven PlatformIO board/toolchain settings.
  - Define a small provider-neutral domain model: `Unknown`, `Clear`, `Alert`, and `Stale` plus timestamps and source health.
  - Implement `TryvohaProvider` as the only initial provider, with the endpoint and regional identifiers in configuration rather than UI logic.
  - Evaluate Dnipro safety conservatively: alert when the oblast is active, Dniprovskyi raion appears in `districts_active`, or the provider reports a relevant partial alert. Do not turn green unless the parsed response explicitly confirms no relevant alert.
  - Poll every 30 seconds after success; use capped exponential reconnect/backoff after failures. Preserve the last alert state briefly for display, but move to `Stale` and extinguish green when freshness expires.
  - Use ESP-IDF HTTP/TLS facilities, cJSON with bounded parsing, `esp_lcd` ST7789 support, and `espressif/led_strip` for the GPIO8 WS2812-compatible LED.
  - Keep Wi-Fi credentials in an ignored local `main/secrets.h` generated from a tracked `main/secrets.example.h`. Add compile-time validation for placeholder values.
  - UI design for the portrait 172 x 320 display: compact top bar (`ДНІПРО`, Wi-Fi indicator); large central shield/status panel; alert duration or update age below; bottom source attribution. Red/green dominate only confirmed alert/clear states; amber is reserved for stale/error/startup.
  - Avoid a geographic map in v1: at this resolution a recognizable accurate map consumes flash and screen area without improving the binary safety decision.
- Risks:
  - The initial provider is unofficial and may be delayed, unavailable, or change its schema.
  - A district name/slug change could cause a false clear unless parsing fails closed; tests must pin representative payloads.
  - ESP32 TLS depends on certificate/time handling and can fail before SNTP synchronization.
  - The product page describes the retail board as 4 MB, while the connected C6FH8 chip is 8 MB; project settings must remain specific to the verified device.
  - Host tests cannot verify LCD orientation, color order, brightness, or Wi-Fi/TLS behavior on real hardware.

## Human decision

| Gate | Status | Date | Decision maker | Decision/rationale | Requested changes |
| --- | --- | --- | --- | --- | --- |
| Solution | approved | 2026-09-06 | user | Approved the Explorer proposal without changes. | None. |
| Test design | approved | 2026-09-06 | user | Approved all thirteen intended host-test behaviors, TDD order, and verification commands without changes. | None. |
| Branch | approved | 2026-09-06 | user | Approved creating `codex/air-siren` from `main` to isolate feature work. | None. |
| Review resolution | approved | 2026-09-06 | user | Approved fixing all three required findings test-first, rerunning verification, and repeating review. | None. |
| Commit | pending |  |  |  |  |
| Acceptance | pending |  |  |  |  |

Physical acceptance found a dark LCD after a successful flash. The official
Waveshare ESP-IDF demo uses a board-specific ST7789T initialization profile at
12 MHz, BGR order, and mirrored X orientation; the implementation used the
generic ST7789 initialization at 40 MHz. This confirmed defect reopens the
feature for a test-first hardware-profile correction.

## TDD test plan

- Test boundary:
  - `main/domain/alert_status.*` contains ESP-independent status types and conservative location evaluation.
  - `main/domain/poll_schedule.*` contains unsigned elapsed-time scheduling and capped failure backoff.
  - `main/providers/tryvoha_parser.*` converts a bounded JSON payload into the provider-neutral result; transport and cJSON allocation stay outside the pure evaluator where practical.
  - `test/host/test_main.cpp` is a dependency-free host executable with explicit named cases and non-zero exit on failure.
  - Test fixtures are short inline JSON strings; tests make no network calls and contain no credentials.
- Intended behavior and initially failing test:
  1. `clear_response_is_clear`: valid response with `active:false`, `active_anywhere:false`, and no active target district yields `Clear`.
  2. `oblast_alert_is_alert`: `active:true` yields `Alert` regardless of the district array.
  3. `dniprovskyi_district_is_alert`: a Dniprovskyi raion entry yields `Alert` when oblast `active` is false.
  4. `unrelated_district_is_not_target_alert`: an unrelated district alone does not create a Dnipro alert, while the response remains valid.
  5. `provider_partial_without_target_detail_is_not_clear`: `active_anywhere:true` without trustworthy target detail yields `Unknown`, never green.
  6. `bad_payloads_fail_closed`: malformed, truncated, oversized, wrong-type, missing-required-field, and contradictory payloads yield `Unknown`.
  7. `status_maps_to_safe_outputs`: `Alert` maps to red LED/red UI, `Clear` to green LED/green UI, and `Unknown`/`Stale` to no green plus amber UI.
  8. `clear_result_expires_to_stale`: a confirmed clear result becomes `Stale` after the configured freshness window and cannot leave green illuminated.
  9. `alert_result_expires_to_stale_without_becoming_clear`: a prior alert that expires is visibly stale and is never interpreted as clear.
  10. `successful_poll_waits_30_seconds`: the scheduler does not request earlier than the documented minimum interval.
  11. `failures_back_off_with_cap`: consecutive failures increase the retry interval and stop at the configured maximum.
  12. `scheduler_handles_tick_rollover`: unsigned tick subtraction produces the correct due/not-due result across wraparound.
  13. `success_resets_failure_backoff`: a valid response restores the normal 30-second polling cadence.
- TDD order:
  - Add the host test executable and the first group of parsing/status tests; run it and record the expected compile/link or assertion failure before production implementation.
  - Implement only enough domain/parser code to pass that group.
  - Add scheduling/freshness tests; record their failure; implement only enough scheduler/state code to pass.
  - Refactor while the full host suite remains green, then integrate ESP-IDF transport, display, RGB LED, and Wi-Fi modules.
- Verification commands from `ESP32/AirSiren/`:
  - Compile host tests: `c++ -std=c++17 -Wall -Wextra -Werror -pedantic -I main test/host/test_main.cpp main/domain/alert_status.cpp main/domain/poll_schedule.cpp main/providers/tryvoha_parser.cpp -o /tmp/airsiren_host_tests`
  - Run host tests: `/tmp/airsiren_host_tests`
  - Firmware build: `/Users/sergejnomerovskij/.platformio/penv/bin/pio run`
  - Hardware checklist: manual after build/upload approval; not part of automated verification.

## Review findings

| Priority | File | Finding | Required resolution |
| --- | --- | --- | --- |
| required — resolved | `ESP32/AirSiren/main/main.cpp` | A previously confirmed `Clear` remained green briefly after Wi-Fi disconnect. | Regression test added; connectivity-aware state now changes prior clear/alert to `Stale` immediately while disconnected. |
| required — resolved | `ESP32/AirSiren/main/platform/wifi_manager.h` | `_connected` crossed FreeRTOS task boundaries as an unsynchronized plain `bool`. | Replaced with `std::atomic_bool` using acquire/release operations. |
| required — resolved | `ESP32/AirSiren/main/providers/tryvoha_parser.cpp` | Boolean prefixes and duplicate status fields could allow malformed JSON to appear valid. | Regression tests added; boolean delimiters and unique required status fields are validated fail-closed. |

## Verification evidence

| Command | Result | Notes |
| --- | --- | --- |
| Initial host compile | expected failure | Production headers/sources did not exist; red phase recorded before implementation. |
| `sh test/run_host_tests.sh` | pass | 13 host tests passed after implementation. |
| `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | ESP32-C6 firmware built; RAM 39,008/327,680 bytes, flash 1,176,794/4,194,304 bytes. |
| Live `curl` to configured regional endpoint | pass | HTTP payload schema and `dnipropetrovska` slug matched the fixture; no credentials sent. |
| Review-regression `sh test/run_host_tests.sh` before fixes | expected failure | Missing connectivity-aware state API proved the regression suite red before correction. |
| Review-regression `sh test/run_host_tests.sh` after fixes | pass | 16 host tests passed, including disconnect, malformed boolean, and duplicate-field cases. |
| Post-review `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | RAM 39,008/327,680 bytes; flash 1,177,078/4,194,304 bytes. |
| Credential-pattern inspection | pass | Only documented placeholder macros/references exist under AirSiren; `main/secrets.h` is ignored. |
| LCD-profile regression test before fix | expected failure | The new Waveshare profile source was absent, proving the hardware-specific test was red before correction. |
| `sh test/run_host_tests.sh` after LCD fix | pass | 17 host tests passed, including the Waveshare ST7789T profile constraints. |
| Post-LCD-fix `/Users/sergejnomerovskij/.platformio/penv/bin/pio run` | pass | Official-style 12 MHz vendor initialization built successfully; RAM 39,008/327,680 bytes, flash 1,177,264/4,194,304 bytes. |

## Handoffs

| Stage | Status | Evidence | Next action |
| --- | --- | --- | --- |
| Installer | accepted | Hybrid scan accepted by user on 2026-09-06. | Explorer proposal. |
| Explorer | accepted | Repository scan, provider documentation, and Waveshare pinout reviewed; solution approved by user on 2026-09-06. | Test-design gate. |
| Test design | accepted | Thirteen dependency-free host behaviors, TDD order, and exact verification commands approved by user on 2026-09-06. | Obtain branch decision before writing tests or application code. |
| Implementer | complete | Red host compile recorded, domain tests green, live schema checked, firmware build green, README added. | Independent code review. |
| Code reviewer | changes required | Three required findings: disconnected-green safety violation, Wi-Fi state data race, permissive malformed JSON acceptance. | Human approves or amends proposed dispositions before bugfix/testing. |
| Review resolution | accepted | User approved all dispositions; regression test failed first, then all three issues were corrected. | Repeat code review. |
| Code reviewer (repeat) | approved | All required findings resolved; no remaining blocker or required findings. | Independent tester validation. |
| Tester | passed with hardware caveat | Host suite, target build, live schema, size, scope, and secret handling verified. LCD orientation/colors, real Wi-Fi recovery, HTTPS on-device, and physical LED colors require hardware execution. | Human accepts outcome or requests changes. |
| Hardware bugfix | ready for device test | Dark-screen report reproduced as a configuration mismatch against Waveshare's official ESP-IDF profile; regression test and corrected vendor sequence are green. | Reflash and visually verify backlight, orientation, colors, and startup screen. |
| Device crash diagnosis | confirmed | USB log shows a stack-protection panic at `app_main()` before display initialization. `AlertClient` placed its 4097-byte response buffer plus other services on the 4096-byte main-task stack. | Move long-lived services to static storage, rebuild, reflash, and repeat USB/visual validation. |
| Device TLS diagnosis | confirmed | After the stack fix, the display renders correctly but the USB log reports `No matching trusted root certificate found`. The service currently uses a WE1 → GTS Root R4 → GlobalSign cross-signed chain. | Enable ESP-IDF certificate-bundle cross-signed-chain verification, rebuild, and validate on device. |

## Knowledge updates

- Current context: FEAT-001 is the active feature.
- Decision records: provider/display decisions will be promoted after final acceptance.
- Architecture/conventions: no update until the proposal is approved.
