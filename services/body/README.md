# Body service — executive summary

## Purpose

The simplest possible demonstration of a body-control service class riding
on the same DDS bus as propulsion/energy — the "door open → cabin light"
demo beat called out explicitly in the flight-plan panel's M3
recommendations. Exists to prove mixed service classes on one bus, not to
model a real body-control ECU.

## Status

Implemented in `apps/body-service/src/main.cpp`. Deterministic square-wave
door cycle (open 3s, closed 5s, repeating every 8s) — no external input, no
randomness, exactly repeatable run-to-run.

## Inputs / outputs

- **Publishes:** `BodyState` (`interfaces/idl/vehicle.idl`) — `door_open`,
  `headlights_on` (always equal to `door_open` — no separate control logic,
  on purpose), `timestamp_ns`.
- **Inputs:** none — internal clock only.

## Owner / node

`body-service` container, logically the cabin zone's body domain.

## QoS and SLA

Default DDS QoS at this milestone. Body-class traffic is the lowest
priority tier in the eventual M4 four-layer prioritization stack (below
propulsion/safety-relevant traffic) — not yet implemented.

## Fault behavior

None implemented yet — same gap as propulsion/energy; not a zone, not yet
tracked by central's discovery module.

## Security assumptions

Same project-wide disclosure: [ADR-0001](../../crosscheck/adr/0001-transport-selection.md).

## API / topics

- DDS topic: `BodyState` (see `interfaces/idl/vehicle.idl`).
- No REST/OpenAPI exposure yet (M5).

## Demo scenarios

- **Door → light**: subscribe to `BodyState` and watch `headlights_on`
  track `door_open` exactly, every 8-second cycle.
- **Verification**: `tools/verify_m3_services.sh` asserts the
  `headlights_on == door_open` invariant holds for every sample in a run
  (469 samples / 11 cycles, last run) and that at least two full cycles are
  observed — not just eyeballed from the logs.
