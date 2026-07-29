# From prototype to production: what would actually need to change

Every item below was deferred deliberately somewhere in this project's
ADRs or docs — this page exists so that list lives in one place instead of
scattered across nine ADRs and four executive summaries. Nothing here is a
surprise; every line links to where it was first disclosed.

This is a portfolio/demo prototype. It was never a goal to make it look
production-ready by omission — the opposite: the gaps below are as
carefully documented as the parts that work.

## Security (highest priority to close first)

- **DDS has no authentication.** Any process on the network can forge
  propulsion/body commands or flood priority traffic (CWE-306, CWE-400 —
  flagged unprompted by the original flight-plan panel).
  [ADR-0001](../crosscheck/adr/0001-transport-selection.md) · needs DDS-Security
  (authentication, access control, encryption) before this leaves an
  isolated network.
- **`api-bridge`'s fault-control endpoints require Docker socket access**
  — root-equivalent on the host, mitigated only by zone-allowlisting and a
  localhost-only assumption.
  [docs/architecture/security-limitations.md](architecture/security-limitations.md)
  · needs a real authenticated, least-privilege orchestration layer in
  front of Docker, not a socket mount, for anything beyond a local demo.
- **No DDS-Security, no TLS, no secrets management** anywhere in the
  stack. All disclosed, none implemented.

## Priority / QoS stack (2 of 4 layers built)

- **Layer 3 (transport priority as real network enforcement)** is set but
  not backed by anything at the network layer on a single-host Docker
  bridge — advisory only today.
- **Layer 4 (DSCP marking + `tc` queueing discipline)** is not built at
  all. [ADR-0006](../crosscheck/adr/0006-priority-stack-and-fault-scenarios.md)
  flags this as the open question worth answering before claiming
  network-level traffic isolation: does it produce a measurable delta over
  layers 1-2 on real hardware?
- **Reliability, deadline, liveliness, and transport-priority are one
  bundled toggle** (`ENABLE_PRIORITY_QOS`) — isolating reliability alone as
  its own experimental variable needs new plumbing
  ([ADR-0009](../crosscheck/adr/0009-benchmark-matrix-scope.md)).

## Protocol / architecture breadth

- **SOME/IP (vsomeip)** — RV Tech's other evaluated protocol — exists only
  as an ADR-level mapping intent, never implemented.
- **Fast DDS comparison** — proposed and dropped in the original debate;
  never benchmarked against Cyclone at matched configuration.
- **ROS 2, Kubernetes, hardware-in-the-loop, multi-node/two-host
  deployment** — all explicitly out of scope from the original flight
  plan, never revisited.
- **A real mini-UDS diagnostics stack** — the current `/api/diagnostics`
  surface is UDS-*inspired* (ReadDTC/ClearDTC in spirit), not a certified
  UDS/ISO 14229 implementation
  ([services/diagnostics](../services/diagnostics/README.md)).

## Measurement rigor

- **Single-host only.** Every latency number in this project depends on
  `CLOCK_MONOTONIC` being shared across containers/processes on one
  machine. A second physical host makes every existing measurement
  invalid without adding PTP synchronization or a round-trip-time
  methodology — neither exists yet
  ([benchmarks/methodology.md](../benchmarks/methodology.md)).
- **No payload/message-size sweep**, despite `load-generator` already
  supporting configurable size and rate — the harness to run and aggregate
  a systematic sweep doesn't exist.
- **No dedicated N-repetition confidence-interval script.** Multiple
  independent runs under different conditions exist (`golden-run-1`,
  `golden-run-2`, native, single-container), which gives some sense of
  run-to-run variance, but that's not the same as repeated trials of one
  identical configuration with a computed interval.
- **The ~100% CPU finding on `central` is unexplained**, not resolved —
  flagged in [ADR-0009](../crosscheck/adr/0009-benchmark-matrix-scope.md)
  as an open question rather than asserted either way without evidence.
- **No CI-enforced performance regression gates.** Deliberately rejected
  as flaky on shared runners early in the flight plan — benchmarks run
  on-demand with committed golden artifacts for manual regression
  comparison instead.

## Crosscheck provenance

- **No measured baseline exists** for the "tokens saved by multi-model
  routing" story — the honesty rules in
  [`crosscheck/README.md`](../crosscheck/README.md) require replaying a
  representative task through a single premium model before publishing
  any efficiency percentage, and that replay was never run. See
  [`crosscheck/reports/case-study.md`](../crosscheck/reports/case-study.md)
  for the full accounting of what is and isn't measured.
- **Session-level token cost for single-model implementation work is not
  metered.** Nine of thirteen ledger entries are logged as "not
  separately metered, primary session" — real, disclosed, and the single
  biggest gap in this project's own cost story.

## Safety and certification

- **Nothing here is safety-qualified.** No ISO 26262 process was
  followed, no ASIL decomposition exists, and no claim in this repository
  should be read as implying otherwise. Every executive summary and README
  status line says so explicitly, on purpose, repeatedly — that repetition
  is intentional, not an oversight.

## How to use this list

If this project continues past M8, this is the backlog — not a wishlist,
a specific set of already-scoped gaps, each with a linked ADR explaining
why it wasn't built the first time. Pick from here before inventing new
scope.
