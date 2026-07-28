# ADR-0002: Docker Compose from day one, via explicit Cyclone DDS unicast peers

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** human decision, reconciling a genuine split between two multi-model debates
- **Ledger ref:** `crosscheck/ledger.jsonl` entries `2026-07-28T-plan-flightplan`, `2026-07-28T-debate-flightplan`

## Context

DDS discovery defaults to multicast, which is unreliable across Docker's
default bridge networks — flagged by every panelist as the top technical
risk in the first slice. The two multi-model debates run for this project
resolved it differently, which is unusual (they agreed on almost
everything else) and is preserved here rather than silently picked.

## Options considered

- **Option A — `plan --thorough` synthesis (0.87 confidence):** keep Docker
  Compose from M0, solve multicast directly with a committed
  `CYCLONEDDS_URI` config listing explicit unicast peers.
- **Option B — `debate --thorough` synthesis (0.86 confidence):** develop
  process-per-zone first (no Docker), containerize only at final packaging
  time using host networking, to avoid the bridge-network risk entirely
  rather than engineering around it.

## Decision

**Option A.** Every provider's "one-command demo" pitch in the original
`confer` round (`docker compose up`) assumes containers are the delivery
mechanism from the start, and the risk is the same top-1 item both debates
flagged for a **day-1 spike** regardless of which option won — meaning
Option A pays down the risk immediately rather than deferring it to a
"final packaging" step that then becomes its own crunch. Validated
2026-07-28: `cyclonedds-dev` 0.10.4 installs cleanly via apt on
`ubuntu:24.04`, `idlc` and CMake config files are present, network egress
from containers is confirmed — no blocking surprises found in the spike.

## Dissent

This is the one point where the two debate syntheses genuinely disagreed
with each other (not just internal panelist dissent) — recorded here as a
human tie-break rather than manufacturing false consensus between two
independent Crosscheck runs. If the day-1 unicast-peer spike had failed,
the fallback was Option B, explicitly, not an ad hoc improvisation.

## Consequences

- `deploy/cyclonedds-peers.xml` (or equivalent) is committed and becomes
  part of the reproducibility story: anyone cloning the repo gets the same
  discovery behavior, not "works on my machine."
- If a two-host or hardware-adjacent demo is later requested, this config
  needs revisiting — same open question the panel flagged.
