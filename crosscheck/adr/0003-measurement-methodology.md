# ADR-0003: Raw-sample capture over HDR-style histogram bucketing

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** single model, bounded implementation (no debate — see rationale)
- **Ledger ref:** `crosscheck/ledger.jsonl` entry `2026-07-28T-m1-measurement-pipeline`

## Context

M1 needed a latency measurement pipeline for Golden Run #1. Every provider
in the original flight-plan panel said "HDR histogram" by name, and the
`plan` synthesis's key claim #9 committed to "HDR histograms, JSONL output"
as part of the M1 exit criterion. Implementing it required picking a
concrete mechanism.

## Options considered

- **HDR-style bucketed histogram** (what the panel named): fixed memory
  regardless of sample count or run duration; standard choice for
  long-running or high-rate production telemetry.
- **Raw sample capture + exact percentile computation** (chosen): store
  every sample, compute percentiles by sorting rather than approximating
  from buckets.

## Decision

Raw capture for now (`middleware/metrics/sample_recorder.hpp` writes JSONL,
`tools/analyze_run.py` computes exact percentiles). This was not escalated
to a panel debate — it is a bounded implementation detail against an
already-frozen goal (produce a percentile report), not an architecturally
uncertain call, so per `crosscheck/MODELS.md` it was routed as a
single-model pass.

Rationale: golden runs at this stage last seconds to low minutes at a few
Hz–kHz, so raw storage is a few thousand to a few hundred thousand samples
— trivially small. At that scale, exact percentiles are both simpler to
implement correctly and strictly more honest than a bucketed
approximation's rounding error. The panel's actual intent — "publish raw
distributions, not adjectives" (independent phrasing from three providers)
— is better served by literally keeping the raw distribution than by
histogramming it prematurely.

## Dissent

None — this was not debated. Flagging that explicitly per ADR-0000's rule
against manufacturing false consensus: this is a human/single-model
engineering call, not a resolved panel disagreement.

## Consequences

- `benchmarks/runs/*.jsonl` (raw dumps) are not committed — regenerable,
  multi-sample intermediate data. `benchmarks/reports/*.{md,json}` are
  committed as the actual evidence.
- **Revisit this decision** the moment a scenario's duration or rate makes
  raw storage impractical (a candidate: M4's sustained congestion/soak
  scenarios, or any run producing tens of millions of samples) — switch to
  HDR-style bucketing there specifically, not project-wide, and log that
  change as its own ADR rather than silently amending this one.
