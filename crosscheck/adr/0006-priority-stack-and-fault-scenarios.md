# ADR-0006: Priority stack scope (M4) and fault-scenario implementation shape

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** single model, bounded implementation — executing the already-decided M4 milestone (ADR-0001's 4-layer priority stack, ADR-0004's deferred DDS liveliness/deadline QoS, and the panel's "exactly 3 deterministic fault scenarios" consensus), not re-debating it
- **Ledger ref:** `crosscheck/ledger.jsonl` entry `2026-07-28T-m4-priority-and-faults`

## Context

M4 is the flight plan's "money milestone": the priority stack and the three
fault scenarios that prove the zonal architecture degrades gracefully
rather than falling over. Two scope questions needed answers before
writing code: how many of the original 4 priority layers to build now, and
whether fault scenarios need a general scenario-runner framework or
purpose-built scripts.

## Decision 1: 2 of 4 priority layers now, 2 stay stretch

Built now:

- **Layer 1 (app-level traffic classes):** `PropulsionState` is treated as
  the high-priority class, `SensorBurst` (the new bulk-telemetry load
  generator topic) as the low-priority class deliberately used to
  congest the bus.
- **Layer 2 (DDS QoS):** `PropulsionState`'s writer/reader use RELIABLE +
  KEEP_LAST(1) + a 150ms deadline + MANUAL_BY_TOPIC liveliness (finally
  landing the DDS liveliness QoS deferred from M2 — ADR-0004) +
  transport_priority set high. `SensorBurst` uses BEST_EFFORT + shallow
  history, on purpose: a stalled or slow consumer must not trigger reliable
  retransmission that competes with propulsion traffic for the same link.

Deferred (stretch, per the original flight plan — not cut, just not core):

- **Layer 3 (transport-priority as a real scheduling signal):** Cyclone's
  `transport_priority` QoS is set and reported, but on a Docker bridge
  network with no DSCP marking or `tc` queueing discipline behind it, nothing
  actually enforces it at the network layer yet. It is measured and
  disclosed as advisory only — exactly the phrase the original flight-plan
  synthesis used for this layer.
- **Layer 4 (DSCP + tc queueing):** not implemented. A real evaluation of
  whether it produces a measurable delta over layers 1-2 on a single-host
  Docker bridge remains an open question from the original synthesis; not
  worth the implementation cost before that question is answered.

**What's actually claimed:** the A/B congestion scenario
(`tools/run_scenario_congestion.sh`) demonstrates that layers 1-2 alone
produce a measurable, honest difference — propulsion latency under a
sensor-burst flood stays much closer to its unloaded baseline when
RELIABLE+deadline+priority QoS is enabled than when propulsion uses the
same best-effort class as the flood. It does not claim network-level
traffic isolation, which would require layer 4.

## Decision 2: one script per fault scenario, not a YAML scenario-runner framework

Three scenarios exist:

1. **Zone kill/restart** — already implemented and verified in M2
   (`tools/smoke-test.sh`); documented here as satisfying this scenario
   rather than re-implemented.
2. **Stale/delayed sensor** — `tc netem` applied live to a running zone
   container (`tools/inject_network_fault.sh` / `tools/clear_network_fault.sh`),
   not an app-level fake delay. Chosen over an app-level shim because it's
   a real, standard Linux mechanism every provider's fault-injection
   recommendation named directly, and it exercises the actual network path
   rather than a code path that only runs during "pretend I'm degraded"
   mode.
3. **Congestion-with-priority-survival** — `tools/run_scenario_congestion.sh`,
   the non-negotiable centerpiece per the panel's debate synthesis.

No declarative YAML scenario engine was built for these three. Per KISS/YAGNI:
three purpose-built scripts, each following the same shape already
established by `tools/run_golden_benchmark.sh` and
`tools/verify_m3_services.sh`, is simpler than a general framework built for
an audience of three. Revisit this the moment a 4th scenario is needed and
the duplication actually hurts — that is the concrete trigger, not a
guess about future needs.

## Dissent

None — not debated, per the same reasoning as ADR-0003/0004/0005.

## Consequences

- Adding layer 4 (DSCP+tc) later does not change the IDL or the app-level
  priority classification — it only changes what enforces the classes at
  the network layer, so this ADR's decisions are additive, not something
  layer 4 would need to unwind.
- If a 4th fault scenario is added and a YAML runner starts looking
  cheaper than a 4th bespoke script, build it then — this ADR's KISS
  argument is time-scoped, not a permanent rule.
