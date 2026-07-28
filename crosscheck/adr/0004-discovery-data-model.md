# ADR-0004: Discovery module data model + deferred DDS liveliness QoS

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** single model, bounded implementation — executing an already-frozen decision (ADR-0001: discovery is a module inside central), not re-debating it
- **Ledger ref:** `crosscheck/ledger.jsonl` entry `2026-07-28T-m2-discovery-topology`

## Context

ADR-0001 already decided *where* discovery lives (a module inside central,
not a standalone broker). M2 needed to decide the concrete data model for
that module: what topics exist, how a zone's health is represented, and
which parts of the panel's "health/liveliness" deliverable land now versus
later.

## Decision

Three topics (`interfaces/idl/discovery.idl`):

- **`CapabilityAnnounce`** — one keyed instance per zone, published at
  startup and re-announced periodically (so central catches up if it joins
  late). Capabilities are a comma-separated string, not an IDL
  `sequence<string>` — nothing here needs structured queries over
  capabilities yet, and the simpler contract is easier to debug by hand.
- **`TopologyState`** — one keyed instance per zone, republished by central
  only on alive/stale transitions (not every tick), so a client subscribing
  to this single topic reconstructs the whole topology without touching raw
  heartbeats.
- **`DiagnosticEvent`** — an append-style event log (info/warning severity)
  emitted on the same transitions. This is a lightweight stepping stone
  toward M5's full mini-UDS diagnostics service, not that service itself.

**Liveliness detection is app-level** (`middleware/health/zone_registry.hpp`:
last-heartbeat timestamp vs. a staleness threshold), not DDS liveliness QoS.
This is a deliberate sequencing choice, not an oversight: the flight plan's
own M4 milestone specifies DDS deadline/liveliness QoS as layer 2 of a
4-layer prioritization stack (ADR-0001) — pulling it into M2 would mean
implementing and validating QoS-triggered instance-liveliness callbacks
under time pressure, for a milestone whose actual exit criterion ("all
zones discover each other, UI/API can show health") app-level staleness
tracking already satisfies on its own.

## Dissent

None — not debated, per the same reasoning as ADR-0003: this is execution
of an already-frozen architectural call, not a new uncertain one.

## Consequences

- M4 revisits this file when DDS liveliness/deadline QoS is added — that
  work supplements (does not replace) `ZoneRegistry`'s app-level tracking,
  since the two answer different questions: DDS QoS gives protocol-level
  instance liveliness state; `ZoneRegistry` gives the specific
  "which zone, since when" data the dashboard and fault scenarios need.
- Adding a 5th zone requires: one more `docker-compose.yml` service block
  (same `zone-runtime` image, new `ZONE_ID`), one more `<Peer>` entry in
  `deploy/cyclonedds-peers.xml`. No new code — this is the "manifest
  configured, not bespoke" payoff from ADR-0001 arriving on schedule.
