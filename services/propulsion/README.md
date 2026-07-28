# Propulsion service — executive summary

## Purpose

Models torque request/delivery and vehicle speed for a single simulated
drive cycle, so the middleware has a real (if illustrative) time-varying
signal to carry, prioritize, and eventually fault-inject around — instead
of a synthetic heartbeat.

## Status

Implemented as part of `energy-service` (see [ADR-0005](../../crosscheck/adr/0005-drive-cycle-model.md)
for why propulsion and energy share one process for now). Splittable into
its own process later if a scenario needs to fault-inject one domain
without the other.

## Inputs / outputs

- **Publishes:** `PropulsionState` (`interfaces/idl/vehicle.idl`) —
  `vehicle_speed_kmh`, `torque_request_nm`, `torque_delivered_nm`,
  `timestamp_ns`. Keyed by `zone_id` (currently always `energy-service`).
- **Inputs:** none external — speed is defined directly as a deterministic
  90-second sinusoidal drive cycle (0 → 60 km/h → 0), not derived from
  sensor fusion. See the model itself for the exact formula.

## Owner / node

`energy-service` container, logically the front zone's propulsion domain.

## QoS and SLA

Default DDS QoS (reliable, volatile) at this milestone. No deadline/priority
QoS yet — that's M4's four-layer prioritization stack, applied uniformly
across the vehicle-domain topics, not specific to propulsion alone.

## Fault behavior

None implemented yet. `energy-service` has no HeartBeat/liveliness of its
own (it's a service, not a zone) and is not yet tracked by central's
discovery module — a candidate for M4/M5 if a fault scenario needs to kill
this specific service.

## Security assumptions

None beyond the project-wide DDS-has-no-auth disclosure in
[ADR-0001](../../crosscheck/adr/0001-transport-selection.md). Torque/speed
values are unauthenticated and unvalidated on the wire, matching every
other topic in this prototype.

## API / topics

- DDS topic: `PropulsionState` (see `interfaces/idl/vehicle.idl`).
- No REST/OpenAPI exposure yet — that's M5.

## Demo scenarios

- **Nominal drive cycle**: subscribe to `PropulsionState` and watch speed
  and torque trace a smooth 90-second accel/decel curve.
- **Verification**: `tools/verify_m3_services.sh` runs one full cycle and
  cross-checks every sample against an independent Python re-derivation of
  the formula — not eyeballed, actually asserted (922 samples, last run).
