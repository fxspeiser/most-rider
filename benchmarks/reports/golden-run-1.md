# Golden run: golden-run-1

Raw samples: `benchmarks/runs/golden-run-1.jsonl` (146 total). Regenerate with `tools/run_golden_benchmark.sh`.

## Latency (microseconds, one-way, front-zone -> central)

| Stat | Value (us) |
|---|---|
| count | 146 |
| min | 52 |
| mean | 145.1 |
| stddev | 68.6 |
| p50 | 123.0 |
| p90 | 218.0 |
| p95 | 245.0 |
| p99 | 320.1 |
| p99.9 | 674.6 |
| max | 733 |

## Samples per zone

| Zone | Samples |
|---|---|
| front-zone | 146 |

## Environment (captured at run time, not hand-entered)

| Field | Value |
|---|---|
| captured_at_utc | 2026-07-28T13:50:38.033121+00:00 |
| orchestrating_host_platform | macOS-26.5.2-arm64-arm-64bit |
| orchestrating_host_machine | arm64 |
| python_version | 3.10.10 |
| docker_version | Docker version 20.10.18, build b40c2f6b5d |
| docker_compose_version | Docker Compose version v5.1.4 |
| docker_daemon_architecture | aarch64 |
| docker_daemon_os | linux / Docker Desktop |
| docker_daemon_cpus | 15 |
| cyclonedds_version | 0.10.4 (Ubuntu 24.04 apt package, see deploy/Dockerfile) |
| measurement_note | Single Docker host; latency is (central's CLOCK_MONOTONIC at receipt) minus (publisher's CLOCK_MONOTONIC at send). Valid because containers on one host share the kernel's monotonic clock (ADR-0002) - NOT valid across two physical hosts without PTP or an RTT methodology. |

## Methodology and scope

No latency target is pre-committed for this run (ADR-0001/plan synthesis: targets are ADR design goals, not public claims until measured). This is a **baseline-derived envelope** — a starting point for M4's priority-under-load comparison and M7's honest benchmark matrix, not an industry comparison. See `benchmarks/methodology.md` for the full disclosure rules.

