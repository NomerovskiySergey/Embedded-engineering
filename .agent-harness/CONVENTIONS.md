# Conventions

Status: Accepted on 2026-09-06.

## Code style

- Use C or C++ supported by the selected ESP-IDF/PlatformIO toolchain.
- Keep hardware and network responsibilities in small modules with explicit initialization and update APIs.
- Use fixed-width integer types where size matters and compile-time constants for pins and intervals.
- Prefer bounded buffers and deterministic ownership; avoid unnecessary heap churn in steady-state firmware.
- Configure hardware pins explicitly before use and document active-high/active-low or addressable LED semantics.
- Poll by elapsed time rather than blocking delays.
- Keep provider-specific JSON/transport details behind a replaceable alert-provider boundary.
- Parse provider payloads with explicit size bounds. Compare JSON location strings only after bounded escape decoding; accept valid upper/lower `\\uXXXX` hex and mixed UTF-8/escaped representations.
- Never embed real credentials or API tokens in tracked files; provide ignored local configuration or provisioning guidance.

## Testing

- Add host-runnable unit tests for provider response parsing and status-state transitions before production implementation.
- Cover active, partial, clear, malformed, HTTP failure, and stale-data behavior.
- Treat “unknown/no data” separately from “confirmed clear.”
- Run `pio run` from the AirSiren project directory as the authoritative firmware build.
- Hardware verification must cover LCD initialization, correct RGB color order, Wi-Fi recovery, and visible stale/error behavior.
- Live threat verification must include Ukrainian location names as actually serialized by the provider, not only decoded test literals.
