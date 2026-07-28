# ADR-0001: Eclipse Cyclone DDS as the sole IPC backbone

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** multi-model debate — 5-round `plan --thorough --super` (weighted confidence 0.87) and independently a 3-round `debate --thorough --super` (weighted confidence 0.86)
- **Ledger ref:** `crosscheck/ledger.jsonl` entries `2026-07-28T-plan-flightplan`, `2026-07-28T-debate-flightplan`

## Context

The zone-to-zone transport is the single most consequential technical
choice in the prototype — it determines latency ceiling, discovery
behavior, container-networking risk, and how credibly the project speaks
the industry's vocabulary. RV Tech's own stated focus is DDS/SOME-IP-based
zonal SOA, so this had to be gotten right and defensible.

## Options considered

- **Eclipse Cyclone DDS** — open-source, Apache-2.0, native QoS (deadline,
  liveliness, transport priority, reliability) mapping directly onto the
  mixed-criticality story, Ubuntu apt package available (`cyclonedds-dev`
  0.10.4, confirmed present on `ubuntu:24.04`), smaller footprint than
  Fast DDS for this scale.
- **eProsima Fast DDS** — comparable DDS implementation; proposed by xai in
  round 1 as offering a performance edge.
- **vsomeip (SOME/IP)** — the other protocol RV Tech evaluates; C++-heavy,
  slower iteration for a first slice, but valuable for SOA-interop breadth.
- **ROS 2** — rejected outright by all panelists as unnecessary runtime
  weight that obscures the middleware story in a first slice.

## Decision

Cyclone DDS is the sole transport for the prototype, behind a thin
transport-abstraction layer so a later comparison spike is possible without
a rewrite. vsomeip is documented as an ADR + IDL-mapping extension path
only — not implemented in v1.

## Dissent

- **xai** argued in round 1 that Fast DDS should be the starting transport
  for a performance edge. **Conceded in round 2**: the claimed advantage is
  unsupported at this payload/topology scale, and Cyclone's smaller image,
  explicit unicast-peer configuration, and easier Python-binding path for
  the scenario harness win on demo velocity. Fast DDS is retained only as
  an **optional post-M7 equivalent-configuration benchmark spike**, cut
  first if time is short.
- No panelist dissented on rejecting vsomeip or ROS 2 as the v1 foundation.

## Consequences

- All zone runtime code is written once against Cyclone's C/C++ API; a
  Fast DDS comparison, if run, reuses the same IDL and scenarios rather
  than a parallel implementation.
- DDS's default lack of authentication (no DDS-Security in v1) is a
  disclosed limitation — see `docs/architecture/security-limitations.md`
  (tracked for M4/M5) — not a claim this prototype makes about production
  readiness.
