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

- **Single Docker host only.** `CLOCK_MONOTONIC` is shared across
  containers because they share the host kernel. The instant a second
  physical host is introduced, this measurement is invalid without a PTP
  sync or a round-trip-time methodology — neither exists yet.
- **Container overhead is included**, not factored out. A number here is a
  "Docker Compose on this machine" number, not a bare-metal number. Any
  native-vs-container comparison must be run and reported explicitly, never
  inferred.
- **No claim of "beats industry"** is made from these reports. A comparison
  against a published figure requires matched hardware and configuration;
  absent that, the only honest claim is against our own prior runs (a
  regression check), stated as "competitive with published figures" at most,
  with the citation named.

## What every report discloses

Generated automatically by `tools/analyze_run.py` — never hand-entered:
host platform/processor, Python/Docker/Compose versions, the Cyclone DDS
version, capture timestamp, and the measurement-validity note above,
alongside count/min/mean/stddev/p50/p90/p95/p99/p99.9/max and per-zone
sample counts.

## Reproducing a report

```bash
./tools/run_golden_benchmark.sh <run-id>
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
