# Model routing policy

How work gets assigned to models in this project. Set once in [ADR-0000](adr/0000-model-routing-policy.md)
and revised there, not here — this file is the current summary, the ADR is
the reasoning.

| Tier | Use for | Example tasks |
|---|---|---|
| **Multi-model debate/plan** (full panel: anthropic, openai, xai, gemini, kimi) | Architecture decisions with genuine uncertainty or high cost of being wrong | Transport selection, QoS/priority design, fault-scenario design, benchmark methodology |
| **Single strong model, one pass** | Bounded implementation with an explicit contract and acceptance test (IDL, API schema) already frozen | Zone runtime code, scenario runner, dashboard components |
| **Cheap/fast model** (`confer_cheap` or equivalent) | Mechanical, low-ambiguity work | Boilerplate, docs formatting, changelog entries, repetitive test scaffolding |
| **Cross-vendor adversarial review** | Anything merged into the safety-relevant or benchmark-claim path | A reviewer from a *different* vendor than the author checks each fault-scenario implementation and every published performance claim |

## Rules

- Escalate to a more expensive tier only after a cheaper tier fails a
  concrete acceptance test — never by default.
- The reviewer for a merged change is never the same vendor as the author,
  when a review is warranted at all.
- Every call is logged to `crosscheck/ledger.jsonl`, expensive or not — the
  point is to show the full distribution, not cherry-pick the impressive
  entries.
- Full-panel debate is reserved for decisions that would be expensive to get
  wrong (see ADR-0000 for the full rationale) — not used as a default for
  every task.
