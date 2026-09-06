# ADR-001: Secondary live threat feed

Status: Accepted on 2026-09-06.

## Context

The keyless regional alert endpoint reports alert/clear state but not whether a current report concerns missiles, guided bombs, or drones. The public Tryvoha live map exposes typed events through an undocumented endpoint.

## Decision

Keep `/api/v1/alerts/dnipropetrovska` as the sole authority for the display theme and red/green LED. Poll `/api/events` over verified TLS only as secondary informational data. Show its bounded, unexpired classification only during an authoritative active alert; otherwise show no threat label. Treat missing or untrusted threat data as `UNKNOWN` rather than changing alert state.

Decode JSON string escapes with bounded, allocation-free logic before matching Dnipropetrovsk oblast or Dnipro, because the live service serializes Cyrillic using `\\uXXXX` escapes.

## Alternatives

- The documented statistics endpoint was rejected because it contains aggregates, not a current threat.
- Token-based alert APIs remain possible future providers but were not required for this keyless implementation.
- Using the threat feed as the alert authority was rejected because it is undocumented and based on automated processing of open-source reports.

## Consequences

The device can display `MISSILE`, `KAB`, `DRONE`, `RECON DRONE`, `WARNING`, `OTHER`, `MULTIPLE`, or `UNKNOWN` without weakening fail-safe alert behavior. Changes to the undocumented event schema may degrade the label to `UNKNOWN`, while the official alert and LED behavior continue independently.

Approved by the user after successful live-device verification on 2026-09-06.
