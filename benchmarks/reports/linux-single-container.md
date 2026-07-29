# Golden run: linux-single-container

Raw samples: `benchmarks/runs/linux-single-container.jsonl` (277 total). Regenerate with `tools/run_golden_benchmark.sh`.

## Latency (microseconds, one-way, front-zone -> central)

| Stat | Value (us) |
|---|---|
| count | 277 |
| min | 20 |
| mean | 82.5 |
| stddev | 59.9 |
| p50 | 73.0 |
| p90 | 135.4 |
| p95 | 174.2 |
| p99 | 282.0 |
| p99.9 | 526.5 |
| max | 529 |

## Samples per zone

| Zone | Samples |
|---|---|
| front-zone | 277 |

## Environment (captured at run time, not hand-entered)

| Field | Value |
|---|---|
| captured_at_utc | 2026-07-28T19:38:13.239328+00:00 |
| run_context | docker-single-container |
| python_version | 3.10.10 |
| cyclonedds_version | 0.10.4 (Ubuntu 24.04 apt package — same image as the docker-compose runs, but central+front-zone run as two processes in ONE container, not two containers on the bridge network) |
| measurement_note | Single host; latency is (central's CLOCK_MONOTONIC at receipt) minus (publisher's CLOCK_MONOTONIC at send). Valid because both processes share the kernel's monotonic clock (ADR-0002) - NOT valid across two physical hosts without PTP or an RTT methodology. |
| orchestrating_host_platform | macOS-26.5.2-x86_64-i386-64bit |
| orchestrating_host_machine | x86_64 |
| docker_version | Docker version 20.10.18, build b40c2f6b5d |
| docker_compose_version | Docker Compose version v5.1.4 |
| docker_daemon_architecture | aarch64 |
| docker_daemon_os | linux / Docker Desktop |
| docker_daemon_cpus | 15 |

## Methodology and scope

No latency target is pre-committed for this run (ADR-0001/plan synthesis: targets are ADR design goals, not public claims until measured). This is a **baseline-derived envelope** — a starting point for M4's priority-under-load comparison and M7's honest benchmark matrix, not an industry comparison. See `benchmarks/methodology.md` for the full disclosure rules.

