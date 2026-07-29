# ADR-0009: M7 benchmark matrix scope, native-comparison design, and the cheap-delegation finding

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** single model, bounded implementation for the tooling; one real `create_cheap` experiment for the documentation-delegation half of M7
- **Ledger refs:** `crosscheck/ledger.jsonl` entries `2026-07-28T-m7-native-and-resource-benchmarks`, `2026-07-28T-m7-cheap-delegation-experiment`

## Context

M7's brief (from the original flight-plan synthesis) was a benchmark matrix
across native-vs-Docker, message sizes, reliability modes, CPU/memory, and
a "cheap-model documentation-delegation experiment... logged in the
ledger." Not all of that was built — this ADR records what shipped, what
was deliberately deferred, and why.

## Decision 1: two native comparisons, not one, because they isolate different variables

- **`tools/run_native_benchmark.sh`** — Cyclone DDS built from source (no
  package manager ships it for macOS — confirmed by trying, not assumed)
  and `central`/`front-zone` run as plain macOS processes. This changes
  **two** variables at once versus the Docker baseline: no containers, and
  a different OS (macOS vs. Linux-in-Docker-Desktop). Disclosed as a
  confound in every report this script produces, not silently presented as
  clean.
- **`tools/run_linux_process_benchmark.sh`** — the single-variable version:
  the same Ubuntu base image and Docker daemon as the standard
  `docker-compose` runs, but `central`/`front-zone` run as two processes in
  **one** container instead of two containers on the bridge network. This
  isolates exactly the "separate containers + bridge network" cost.
  Result: p50 ~73-75us here vs. ~114-123us for the standard docker-compose
  golden runs — a real, repeatable effect attributable to one specific
  cause, not a platform swap.

`analyze_run.py` gained a `--context` flag (`docker-compose` /
`docker-single-container` / `native`) so each report's environment section
discloses what's actually authoritative for that run — the alternative
(reuse the Docker-assuming environment code unconditionally) produced a
genuinely misleading first native report, caught by reading it.

## Decision 2: CPU/memory via `docker stats`, not new C++ instrumentation

`tools/sample_docker_stats.py` polls `docker stats --no-stream` during a
benchmark run rather than adding resource-accounting code to
`central`/`zone-runtime` — the daemon already has this data. Two real bugs
surfaced while building it (zero-memory samples from polling a not-yet-
running container, and a duration loop that assumed 1-second increments
regardless of actual `docker stats` call latency, silently under-sampling)
— both fixed, documented in the ledger, not swept past.

The resulting finding is disclosed, not resolved: `central` measured near
100% CPU utilization even under this light workload. This is left as an
**open question** (Cyclone DDS's internal receive-thread model plausibly
favors latency over CPU efficiency by design — but that's a hypothesis,
not a verified explanation) rather than asserted as either "expected" or
"a bug" without evidence either way.

## Decision 3: the cheap-delegation experiment's real result was a correct rejection

`benchmarks/EXECUTIVE_SUMMARY.md` was delegated to `crosscheck`'s
`create_cheap` first, grounded in the seven real committed benchmark
reports. Two findings, both kept rather than quietly retried away:

1. **`create_cheap` is not a single cheap-tier call.** It ran a full
   7-provider scope panel and a full 7-provider review panel plus a 7-node
   DAG — $0.58 / 73,603 tokens for one page. Per-node model selection did
   bias toward cheaper tiers for mechanical steps, but the name doesn't
   mean "trivially cheap" in absolute terms, and reporting otherwise would
   misrepresent the tool.
2. **The draft was correctly rejected.** An internal DAG step
   (`fact_ledger`) reported it could not access the seven attached
   documents, despite the orchestrator's own document-ingestion step
   succeeding for all seven (confirmed via hashes/byte counts in the raw
   response) — a real gap between ingestion and sub-task context wiring.
   The draft self-disclosed this with a verification-notice banner and
   `[MISSING: ...]` markers rather than fabricating numbers, and the
   `final_gate` reviewer (`claude-opus-5`) issued a "do not ship this"
   verdict citing exactly that. The flawed draft was discarded;
   `benchmarks/EXECUTIVE_SUMMARY.md` was rewritten directly, grounded in
   the same seven reports read directly.

This is treated as a successful demonstration of the review layer, not a
failed delegation — a system that always produces clean cheap-tier output
would be less credible evidence than one that visibly catches its own
failures before they ship.

## What's explicitly deferred (not built, not silently dropped)

- **Payload/message-size sweep** — `load-generator`'s `PAYLOAD_BYTES` and
  `RATE_HZ` already support this; the harness to run a systematic sweep and
  aggregate the results does not exist yet. The M4 congestion scenario
  covers one size-vs-load data point, not a full sweep.
- **Repeated-run confidence intervals via a dedicated N-repetition
  script** — not built. Partial mitigation: this milestone's reports
  (`golden-run-1`, `golden-run-2`, `native-macos-arm64`,
  `linux-single-container`) already function as independent repeated
  observations under different conditions, giving some sense of run-to-run
  variance, but this is not the same as N repetitions of the *identical*
  configuration with a computed confidence interval.
- **Reliable-vs-best-effort as an isolated variable** — `ENABLE_PRIORITY_QOS`
  (ADR-0006) bundles reliability with deadline/liveliness/priority as one
  toggle; isolating reliability alone would need new plumbing not built
  here.

## Consequences

- Anyone re-running `tools/run_native_benchmark.sh` on a fresh machine
  pays a one-time ~5s Cyclone DDS source-build cost (cached at
  `.native-bench-cache/`, gitignored) — acceptable, since it was confirmed
  fast by actually timing it, not assumed.
- The three deferred items above are the natural next slice of M7 if this
  project continues past M8 — logged here so "benchmark matrix" isn't
  quietly redefined down to mean only what got built.
