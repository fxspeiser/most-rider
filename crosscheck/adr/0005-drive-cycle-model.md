# ADR-0005: Analytic drive-cycle model, combined propulsion+energy process

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** single model, bounded implementation — executing the M3 milestone's named deliverable (ADR-0001/plan synthesis: "energy service must never be cut"), not an architecture debate
- **Ledger ref:** `crosscheck/ledger.jsonl` entry `2026-07-28T-m3-vehicle-services`

## Context

M3 needed a powertrain/energy vertical slice and a body-control demo beat.
Two implementation questions needed answers before writing code: how to
generate a repeatable drive cycle without a tuning-heavy physics
integrator, and whether propulsion and energy are one process or two.

## Decision 1: analytic, not integrated, drive cycle

Vehicle speed is defined **directly** as a smooth sinusoid
(`speed_kmh(t) = 60 * 0.5 * (1 - cos(2*pi*t/90))`), and torque/power are
derived from its **exact analytic derivative** — not by integrating
acceleration forward from noisy or estimated inputs. This was chosen over
a forward physics integrator (accelerator input → force → integrate
acceleration → velocity) because:

- It is exactly reproducible from `t` alone — no accumulated floating-point
  drift, no integrator instability to tune under time pressure.
- The torque sign is always analytically consistent with the speed curve's
  direction (accelerating vs. decelerating), which is what actually needs
  to be true for the regen-braking demo beat to work.
- It is independently re-derivable in a second language for verification —
  which `tools/verify_m3_services.py` does: every logged sample is checked
  against a Python reimplementation of the same formula, not just eyeballed
  for plausibility. Last run: 922/922 samples matched within tolerance.

**What's tuned vs. physically meaningful:** wheel radius (0.33m) and
battery capacity (75 kWh) are realistic EV values. `kInertiaTorqueScale`
(900 Nm per m/s²) is **not** a calibrated vehicle mass — it's picked so
torque/power numbers land in a plausible EV range. This is disclosed in the
code comment and here, not silently presented as calibrated.

## Decision 2: propulsion + energy in one process

`energy-service` publishes both `PropulsionState` and `EnergyState` from
one drive-cycle clock, rather than two processes with one subscribing to
the other's torque signal. The two domains are tightly physically coupled
(torque × angular velocity = power, directly) — splitting them into
separate processes now would add an inter-process dependency and a
discovery-ordering concern for no present benefit. `services/propulsion/README.md`
and `services/energy/README.md` document them as separate services
regardless of this implementation detail, so splitting later is a
refactor, not a contract change.

## Dissent

None — not debated, per the same reasoning as ADR-0003/ADR-0004: this
executes an already-named deliverable, not a new uncertain architectural
call.

## Consequences

- `energy-service` and `body-service` are **services, not zones**: no
  `HeartBeat`/`CapabilityAnnounce`, not yet tracked by central's discovery
  module. If M4's fault scenarios need to kill one of these specifically,
  that tracking gets added then, not speculatively now.
- If a future milestone needs propulsion and energy to fault-inject
  independently (e.g., "energy service crashes, propulsion keeps running"),
  split them into two processes at that point — the IDL contracts already
  support it since they're independent topics.
