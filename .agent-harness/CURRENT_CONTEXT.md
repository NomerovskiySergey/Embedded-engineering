# Current context

## Active feature

None. `FEAT-002-threat-type-display.md` is accepted and complete; Git commit remains pending explicit user approval.

## Confirmed project knowledge

- Installation status: accepted by the user on 2026-09-06
- Knowledge mode: hybrid
- Target application path: `ESP32/AirSiren/`
- Target hardware: Waveshare ESP32-C6-LCD-1.47 with ESP32-C6FH8 and 8 MB flash
- Toolchain: C/C++, PlatformIO, ESP-IDF 6.0.1
- Authoritative build command: `pio run`
- Target location: Dnipro, Dnipropetrovsk oblast
- Last updated: 2026-09-06

## Draft scan findings

- Draft — needs confirmation: sibling project `ESP32/LedBlinking/` is the hardware/toolchain reference.
- Draft — needs confirmation: its PlatformIO environment is `waveshare_esp32_c6_lcd` using `esp32-c6-devkitc-1` and `espressif32@7.0.1`.
- Draft — needs confirmation: the onboard addressable RGB LED is on GPIO8 and uses RGB component order.
- Draft — needs confirmation: Wi-Fi credentials and future API configuration must not be committed in application sources.

## Recent decisions

- User selected hybrid initialization and authorized a read-only feature inventory.
- User accepted the initial knowledge base on 2026-09-06.
- User accepted Tryvoha.online as the initial keyless provider, with a replaceable adapter that can later support NEPTUN.
- User approved the FEAT-001 Explorer solution on 2026-09-06 without changes.
- User approved the FEAT-001 TDD test design on 2026-09-06 without changes.
- User approved creating `codex/air-siren` from `main` on 2026-09-06.
- Implementer recorded a red compile before production code; 13 host tests and the ESP32-C6 build now pass.
- Code review found three required issues: disconnected-green safety behavior, an event-task data race, and permissive malformed JSON parsing.
- User approved all review dispositions; regression tests were red before fixes and 16/16 now pass.
- Repeat code review found no blocker/required findings. Firmware build passes with 39,008 bytes static RAM and 1,177,078 bytes flash usage.
- FEAT-002 adds an informational `/api/events` threat feed while preserving the official regional endpoint as the sole alert/LED authority.
- A physical-board defect caused escaped Ukrainian location names to remain `UNKNOWN`; bounded JSON Unicode decoding fixed it test-first.
- User flashed the corrected build and confirmed live threat classification works on 2026-09-06. Final evidence: 26 host tests, RAM 71,808 bytes, flash 1,182,014 bytes.

## Next action

Create a Git commit for the accepted AirSiren work only if the user explicitly approves committing it.
