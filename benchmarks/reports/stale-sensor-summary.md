# Stale/delayed-sensor scenario: verdict

**PASS**

- Injected delay: 300.0ms (300000us)
- Baseline mean latency: 134.1us
- Faulted mean latency: 304682.1us (+304548us vs baseline)
- Recovered mean latency: 123.2us (delta vs baseline: -11us)

- Fault detected in measured latency: yes (observed increase must exceed 50% of injected delay)
- Recovery confirmed after clearing fault: yes

## Full stats

| Phase | count | mean | p50 | p99 |
|---|---|---|---|---|
| baseline | 75 | 134.1 | 116.0 | 278.2 |
| faulted | 68 | 304682.1 | 304870.0 | 324436.8 |
| recovered | 75 | 123.2 | 118.0 | 224.7 |

## Method

Real `tc netem` delay applied to a running zone container's network
interface (tools/inject_network_fault.sh) — not an app-level fake. The
injected delay is one-way and should appear directly in measured
latency (central_receive_ns - sender_send_ns), since both share the
host's monotonic clock (ADR-0002).

