# Congestion-with-priority-survival: verdict

**PASS** — Under the same flood, priority QoS held p99 degradation to -43.3us vs 341.8us without it — priority measurably helped.

**Caveat, disclosed rather than omitted:** while p99 improved, the far tail did not — p99.9 was 9632.3us with priority QoS vs 6450.3us without it, and max was 30609us vs 8698us. RELIABLE QoS likely trades occasional ACKNACK-retransmission stalls for a better typical-tail (p99) outcome. Investigate before claiming priority QoS improves *every* percentile — it does not, in this run.

## p99 latency by phase (microseconds)

| Phase | p99 (us) | vs. baseline |
|---|---|---|
| baseline (no flood, priority QoS on) | 498.1 | — |
| no-priority (flood on, priority QoS off) | 839.9 | +341.8 |
| priority (flood on, priority QoS on) | 454.8 | +-43.3 |

## Full stats per phase

| Phase | count | p50 | p90 | p95 | p99 | p99.9 | max |
|---|---|---|---|---|---|---|---|
| baseline | 1425 | 88.0 | 158.6 | 227.8 | 498.1 | 3059.5 | 8474 |
| no-priority | 1422 | 79.0 | 158.0 | 215.9 | 839.9 | 6450.3 | 8698 |
| priority | 1414 | 96.0 | 174.4 | 246.3 | 454.8 | 9632.3 | 30609 |

## Scope

This tests layers 1-2 of the priority stack (app-level traffic class +
DDS RELIABLE/deadline/KEEP_LAST QoS) — see ADR-0006. It does not test
layer 4 (DSCP + tc queueing), which is deferred. `transport_priority`
(layer 3) is set but not claimed to enforce anything at the network
layer on this single-host Docker bridge — advisory only.

