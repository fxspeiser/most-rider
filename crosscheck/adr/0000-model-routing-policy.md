# ADR-0000: Model routing policy

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** multi-model confer (5-provider single-shot panel: anthropic, openai, xai, gemini, kimi)
- **Ledger ref:** `crosscheck/ledger.jsonl` entries `2026-07-28T-confer-flightplan`

## Context

This project has a dual mission: build a credible zonal-architecture
middleware prototype, and demonstrate that Crosscheck's multi-model
orchestration produces better engineering economics than a single-model
workflow. That second goal only lands if the routing decisions are
principled and disclosed — not "we used five models on everything," which
would actually be the failure mode the project is arguing against.

## Options considered

- **Full panel on every task.** Rejected by all five panelists independently
  — maximizes token spend without a proportional quality gain on mechanical
  work, and undermines the efficiency claim.
- **Single model for everything, no debate.** Rejected — throws away the one
  capability (adversarial cross-vendor review, architecture debate) that is
  actually differentiating for the high-stakes decisions.
- **Task-tiered routing** (this decision) — full panel only for decisions
  with genuine uncertainty or high cost-of-being-wrong; single model for
  bounded implementation against a frozen contract; cheap model for
  mechanical work; cross-vendor review only on the safety-relevant/
  benchmark-claim path.

## Decision

Tiered routing as specified in `crosscheck/MODELS.md`. Concretely for this
project: the M0–M8 flight plan itself (this document's parent decision) was
produced via full-panel `confer --super`, 5-round `plan --thorough`, and
3-round `debate --thorough` — three calls, $5.24, ~555K tokens, because
getting the architecture right up front is the single highest-leverage
place to spend. Everything downstream defaults to single-pass or cheap-tier
unless a specific decision meets the "genuine uncertainty, high cost of
being wrong" bar.

## Dissent

No panelist argued against tiered routing itself — the debate was about
*where the tier boundaries sit*, which is captured per-decision in later
ADRs rather than here.

## Consequences

- The ledger will show a small number of expensive entries (architecture)
  and a long tail of cheap ones (implementation) — that shape is itself
  evidence for the pitch, not a metric to optimize away.
- Any reviewer of this repo can audit whether a given piece of work was
  routed at the right tier by checking its ledger entry against this policy.
