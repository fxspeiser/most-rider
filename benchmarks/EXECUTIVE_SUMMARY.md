# Executive summary — benchmark findings

*Prototype/demo evidence only. Not certified, safety-qualified, or vehicle-ready software.*

## What was tested

In a zonal vehicle architecture, compute is grouped by physical area of the
car rather than by function, so almost everything depends on messages
arriving from a zone to a central controller on time — even under network
congestion or a partial fault. This project measured that timing across
several setups (with and without containers, with and without heavy
background traffic) and under two injected faults: a zone going silent, and
a sensor's data arriving late.

The number that matters most to a systems team isn't the average — it's
how bad the slowest messages get when things go wrong, since that's what
determines whether a safety-relevant signal still arrives in time.

## Headline results

- **Typical latency:** front-zone-to-central message delivery measured
  around 75-155 microseconds (millionths of a second) at the midpoint
  across four different runs (`golden-run-1`, `golden-run-2`,
  `native-macos-arm64`, `linux-single-container`).
- **Removing the container network hop helped measurably.** Running the
  same two processes in a single container instead of two separate
  containers on a virtual network dropped the midpoint latency from
  ~114-123us to ~73-75us — a real, repeatable effect of eliminating one
  network layer, not noise.
- **Prioritized traffic survives congestion better, on the primary
  metric.** Under an artificial flood of low-priority traffic, the
  99th-percentile latency of the high-priority signal was 454.8us with
  priority handling on, versus 839.9us with it off — a real, measured
  improvement under the same load.
- **A real network fault showed up exactly where expected.** Injecting a
  300-millisecond network delay onto one zone produced a measured latency
  increase of ~304.5 milliseconds — matching the injected delay almost
  exactly — and recovery to baseline within seconds of clearing it.

## The honest caveats — not smoothed over

- **Prioritization's benefit doesn't extend to the extreme tail.** While
  the 99th-percentile latency improved with priority handling on, the
  worst-case (max) observed latency was *higher* with priority on (30.6ms)
  than without it (8.6ms) — likely a side effect of the reliable-delivery
  QoS setting retrying delivery under heavy load. Priority handling is not
  a universal win at every percentile, and we say so rather than only
  reporting the metric that looks good.
- **The native-vs-container comparisons aren't single-variable.** The
  macOS-native run changed both the operating system *and* removed
  containers at once (two confounded variables); the single-container run
  isolates just the container/network variable but required building
  Cyclone DDS from source rather than the same packaged version used
  everywhere else. Both comparisons are disclosed with their limits, not
  presented as a clean apples-to-apples result.
- **Resource usage (CPU/memory) was only measured for one run so far**,
  and it surfaced a real, currently-unexplained finding: the central
  process ran at close to 100% CPU utilization even under this light
  workload. We don't yet know if that's expected behavior from the
  underlying messaging library's internal thread design or something worth
  investigating further — flagged as an open question, not resolved here.

## What this demonstrates

This project used a mix of routing strategies to build and validate itself:
most implementation work was done directly and verified by actually
running it (every benchmark above was executed, not estimated). For this
specific summary, a cheaper/faster AI model was deliberately tried first,
with a panel of other models reviewing its work before it shipped. That
review caught a real problem — an internal step in the cheap-model pipeline
failed to actually read the source data it was summarizing, and produced a
draft with unverified numbers and placeholder gaps. The review correctly
blocked that draft rather than letting it ship (see
`crosscheck/ledger.jsonl`, entry `2026-07-28T-m7-cheap-delegation-experiment`,
for the full account). This summary was rewritten directly against the
underlying reports afterward. The result is a concrete example of the
review layer doing its job — catching an unreliable output before it
reached a reader — which is arguably a more useful demonstration than a
clean success would have been.
