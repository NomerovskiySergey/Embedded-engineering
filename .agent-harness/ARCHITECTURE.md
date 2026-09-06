# Architecture

Status: Accepted on 2026-09-06.

## System map

- The repository groups embedded exercises/projects by target family (`ArduinoUNO/`, `ESP32/`, `Nanit/`).
- Each ESP32 application is a self-contained PlatformIO project.
- `ESP32/LedBlinking/` is the existing reference for the connected Waveshare ESP32-C6-LCD-1.47 board.
- `ESP32/AirSiren/` is isolated from sibling applications.
- Authoritative flow: Wi-Fi -> verified HTTPS `/api/v1/alerts/dnipropetrovska` -> parsed alert state -> LCD theme and RGB LED.
- Informational flow: verified HTTPS `/api/events` -> bounded event parser -> current threat label shown only while the authoritative state is Alert.
- Backlight flow: synchronized epoch -> Kyiv local time -> pure brightness policy -> cached LEDC duty on GPIO22. This flow does not suspend networking, polling, display control, or the status LED.

## Invariants

- Do not modify sibling applications for AirSiren work.
- Keep secrets and Wi-Fi credentials out of tracked source files.
- The main task loop must be non-blocking and tolerate `millis()`/tick rollover.
- Network failure must be represented as unknown/stale, never as “no alert.”
- Red means an active or partial alert affecting the configured location; green means a fresh confirmed clear state.
- An unavailable/stale state must not silently illuminate green.
- The device is supplementary and must not be presented as an official life-safety warning channel.
- Threat classification must never change the authoritative alert state or LED output.
- Alert always overrides scheduled dimming to normal brightness. Invalid time and timezone setup fail visibly at normal brightness.
