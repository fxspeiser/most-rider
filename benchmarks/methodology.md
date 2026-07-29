# Benchmark methodology

Rules every report under `benchmarks/reports/` must follow. Written before
Golden Run #1 so the first report is held to the same bar as the last.

## What is measured

One-way latency from a zone's DDS `dds_write` to `central`'s `dds_take`,
computed as `central_receive_monotonic_ns - publisher_send_monotonic_ns`.
This is **message latency only** — it is not fault-detection time and not
service-recovery time (M4 introduces both; they are reported separately
when they exist, never folded into this number).

## Why raw samples, not HDR histograms

Every sample is captured (`middleware/metrics/sample_recorder.hpp`) and
percentiles are computed exactly from the sorted raw array
(`tools/analyze_run.py`), not from a bucketed approximation. At the sample
counts a golden run produces (seconds to low minutes at a few Hz–kHz),
exact computation is both simpler to verify and strictly more honest than
an HDR-style histogram's bucket rounding. See
[ADR-0003](../crosscheck/adr/0003-measurement-methodology.md). This is
revisited if a scenario's duration or rate makes raw storage impractical.

## Validity boundary — read this before trusting a percentile

- **Single host only.** `CLOCK_MONOTONIC` is shared across containers (or
  processes) because they share the host kernel. The instant a second
  physical host is introduced, this measurement is invalid without a PTP
  sync or a round-trip-time methodology — neither exists yet.
- **Container overhead is disclosed via `--context`, not inferred.**
  `tools/analyze_run.py --context {docker-compose,docker-single-container,native}`
  changes which environment facts a report treats as authoritative (M7,
  ADR-0009) — a native run has no Docker daemon to query, and reusing the
  Docker-assuming environment code unconditionally produced a genuinely
  misleading first native report (it claimed Docker facts about a run that
  used no Docker at all). See [the native-comparison matrix](#native-vs-docker-comparisons-two-of-them-on-purpose)
  below.
- **No claim of "beats industry"** is made from these reports. A comparison
  against a published figure requires matched hardware and configuration;
  absent that, the only honest claim is against our own prior runs (a
  regression check), stated as "competitive with published figures" at most,
  with the citation named.

## What every report discloses

Generated automatically by `tools/analyze_run.py` — never hand-entered:
context-appropriate host/Docker-daemon facts, the Cyclone DDS version and
provenance (apt package vs. built from source — these are *not* the same
binary and a report must say which), capture timestamp, and the
measurement-validity note above, alongside
count/min/mean/stddev/p50/p90/p95/p99/p99.9/max and per-zone sample counts.

## Native-vs-Docker comparisons — two of them, on purpose

| Script | Isolates | Confound |
|---|---|---|
| `tools/run_golden_benchmark.sh` | baseline: docker-compose, separate containers on the bridge network | — |
| `tools/run_linux_process_benchmark.sh` | separate-containers/bridge-network overhead specifically | none — same OS, same Docker daemon, same base image |
| `tools/run_native_benchmark.sh` | "no containers at all" | **confounded**: also changes OS (macOS vs. Linux-in-Docker) and Cyclone DDS provenance (built from source vs. apt package) |

Read both reports' own disclosed caveats before quoting either number out
of context — see [ADR-0009](../crosscheck/adr/0009-benchmark-matrix-scope.md).

## CPU/memory capture

`tools/sample_docker_stats.py` polls `docker stats` during a run (no new
C++ instrumentation — the daemon already has this) and writes
`benchmarks/reports/<run-id>-resources.json` alongside the latency report.
Wired into `tools/run_golden_benchmark.sh` by default. A live process never
reports exactly 0 memory or CPU for its entire runtime — treat any report
showing that as a sampling artifact, not a real reading (see ADR-0009 for
the two real bugs this caught during development).

## Reproducing a report

```bash
./tools/run_golden_benchmark.sh <run-id>                 # docker-compose baseline (+ CPU/memory)
./tools/run_linux_process_benchmark.sh <run-id>          # single-container, isolates bridge-network cost
./tools/run_native_benchmark.sh <run-id>                 # native (macOS) — confounded, see table above
# or, with a custom duration:
GOLDEN_RUN_DURATION_S=60 ./tools/run_golden_benchmark.sh golden-run-2
```

Writes `benchmarks/runs/<run-id>.jsonl` (raw samples, gitignored — see
below) and commits-worthy `benchmarks/reports/<run-id>.{md,json}`.

## What's committed vs. regenerated

`benchmarks/reports/*.{md,json}` are committed — they are the demo-insurance
artifacts the flight plan calls for (a fallback if a live run misbehaves,
and the input to M6's dashboard). `benchmarks/runs/*.jsonl` (the raw sample
dumps) are not committed — they're regenerable multi-megabyte intermediate
data, not evidence in themselves; the report is the evidence.
`.native-bench-cache/` (the source-built Cyclone DDS used by
`run_native_benchmark.sh`) is likewise gitignored and rebuilt on first use.

## What's deferred (M7 scope, see ADR-0009)

A message/payload-size sweep, a dedicated N-repetition confidence-interval
script, and isolating reliable-vs-best-effort as its own variable
(currently bundled into `ENABLE_PRIORITY_QOS`, ADR-0006) are not built.
Logged explicitly rather than silently redefining "benchmark matrix" down
to mean only what shipped.
