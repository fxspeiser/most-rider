# Golden run: native-macos-arm64

Raw samples: `benchmarks/runs/native-macos-arm64.jsonl` (278 total). Regenerate with `tools/run_golden_benchmark.sh`.

## Latency (microseconds, one-way, front-zone -> central)

| Stat | Value (us) |
|---|---|
| count | 278 |
| min | 73 |
| mean | 164.6 |
| stddev | 58.2 |
| p50 | 156.0 |
| p90 | 200.0 |
| p95 | 225.1 |
| p99 | 295.5 |
| p99.9 | 805.4 |
| max | 963 |

## Samples per zone

| Zone | Samples |
|---|---|
| front-zone | 278 |

## Environment (captured at run time, not hand-entered)

| Field | Value |
|---|---|
| captured_at_utc | 2026-07-28T19:36:03.485180+00:00 |
| run_context | native |
| python_version | 3.10.10 |
| cyclonedds_version | 0.10.4 (built from source, github.com/eclipse-cyclonedds/cyclonedds, not the Ubuntu apt package used in Docker runs) |
| measurement_note | Single host; latency is (central's CLOCK_MONOTONIC at receipt) minus (publisher's CLOCK_MONOTONIC at send). Valid because both processes share the kernel's monotonic clock (ADR-0002) - NOT valid across two physical hosts without PTP or an RTT methodology. |
| host_platform | macOS-26.5.2-x86_64-i386-64bit |
| host_machine | x86_64 |

## Methodology and scope

No latency target is pre-committed for this run (ADR-0001/plan synthesis: targets are ADR design goals, not public claims until measured). This is a **baseline-derived envelope** — a starting point for M4's priority-under-load comparison and M7's honest benchmark matrix, not an industry comparison. See `benchmarks/methodology.md` for the full disclosure rules.

