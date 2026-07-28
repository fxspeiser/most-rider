# Energy service — executive summary

## Purpose

Models battery state of charge, instantaneous power flow (consumption and
regenerative charging), and range estimate — derived from the propulsion
service's torque/speed signal within the same process. This is the
project's named "powertrain/energy vertical slice" deliverable
(`project_overview.md`) — the one service the flight plan says must never
be cut.

## Status

Implemented in `apps/energy-service/src/main.cpp`. Deliberately simple and
labeled illustrative — see [ADR-0005](../../crosscheck/adr/0005-drive-cycle-model.md)
for exactly which constants are physically meaningful (wheel radius,
battery capacity) versus tuned only to keep numbers in a plausible range
(the torque/inertia scale factor).

## Inputs / outputs

- **Publishes:** `EnergyState` (`interfaces/idl/vehicle.idl`) —
  `battery_soc_pct`, `power_draw_kw` (positive = consuming, negative =
  regen charging), `range_estimate_km`, `timestamp_ns`.
- **Inputs:** none external — computed from the same internal drive-cycle
  clock as `PropulsionState`, in the same process, every publish tick.

## Owner / node

`energy-service` container.

## QoS and SLA

Default DDS QoS at this milestone; no deadline/priority QoS applied yet
(M4).

## Fault behavior

None implemented yet — same gap as propulsion (see that service's summary).
SoC is clamped to `[0, 100]` as an illustrative cap, not a real BMS
charge-limiting curve.

## Security assumptions

Same project-wide disclosure as propulsion:
[ADR-0001](../../crosscheck/adr/0001-transport-selection.md).

## API / topics

- DDS topic: `EnergyState` (see `interfaces/idl/vehicle.idl`).
- No REST/OpenAPI exposure yet (M5).

## Demo scenarios

- **Consumption vs. regen**: during the drive cycle's acceleration half,
  `power_draw_kw` is positive and SoC decreases; during the deceleration
  half, `power_draw_kw` goes negative (regen) and SoC recovers slightly —
  observed and logged in `crosscheck/ledger.jsonl` (M3 entry), e.g.
  `soc=79.93% -> 79.94%` across a measured deceleration window.
- **Verification**: same `tools/verify_m3_services.sh` run as propulsion —
  every sample's derived values are cross-checked against the documented
  formula, and SoC bounds are asserted, not eyeballed.
