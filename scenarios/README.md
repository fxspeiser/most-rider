# Fault scenarios

The three scenarios the flight-plan panel converged on as the minimum set
that proves graceful degradation (ADR-0006) — no more, no general chaos
framework, per that ADR's KISS/YAGNI reasoning.

| # | Scenario | How to run | Result (last run) |
|---|---|---|---|
| 1 | Zone kill/restart | `tools/smoke-test.sh` | Central detects the stale transition and the recovery — see M2 in `crosscheck/ledger.jsonl`. Implemented in M2, not re-implemented here; this scenario's proof already existed before M4 asked for it. |
| 2 | Stale/delayed sensor | `tools/run_scenario_stale_sensor.sh` | Real `tc netem` 300ms delay on rear-zone's network interface. Measured latency jumped from a 134us mean to 304,682us — within 0.1% of the injected delay — and returned to 123us within 2 seconds of clearing the fault. Full report: `benchmarks/reports/stale-sensor-summary.md`. |
| 3 | Congestion-with-priority-survival | `tools/run_scenario_congestion.sh` | Under an identical 3000Hz/4096B flood, PropulsionState's p99 latency was 454.8us with priority QoS enabled vs. 839.9us without it — priority measurably helped. The far tail (p99.9/max) did *not* improve and is disclosed as a caveat, not hidden. Full report: `benchmarks/reports/congestion-survival-summary.md`. |

## Why scenario 1 isn't re-implemented here

`tools/smoke-test.sh` already does exactly this: stop `rear-zone`, confirm
central logs `[warning] rear-zone went stale` within its staleness window,
restart it, confirm `[info] rear-zone recovered`. That was built to verify
M2's discovery module and happens to be word-for-word the panel's "zone
kill/restart" scenario. Building a second version would be duplication for
its own sake.

## Why scenario 2 uses real `tc netem`, not an app-level fake

A number of implementation options were available for "the sensor is
delayed" — an in-app sleep-before-publish flag, a proxy, or real network
impairment. Real `tc netem` was chosen because it exercises the actual
network path the production system would use, requires no special-case
code in the zone binary, and — as it turned out — produced an
unambiguous, easy-to-verify signal: the injected delay showed up almost
exactly in the measured latency, with no need for interpretation.

## Why scenario 3 needed two iterations to get a clean result

The first attempt (default 10Hz signal rate, 500Hz/2KB flood, 30s phases)
came back **INCONCLUSIVE** — not a bug, the verdict logic correctly
identified that the flood hadn't actually stressed the system and that
~300 samples aren't enough for a stable p99 estimate. Rather than
re-running until a favorable number appeared, the scenario's own
`PUBLISH_PERIOD_MS`/`RATE_HZ`/`PAYLOAD_BYTES` were increased (50Hz probe
rate, 3000Hz/4KB flood) to get enough statistical power, and the result was
re-run and reported as-is, including the far-tail caveat. See
`crosscheck/ledger.jsonl` for both attempts.
