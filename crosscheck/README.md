# The `crosscheck/` provenance layer

This directory is not documentation about the project — it **is** the second
product. Everything the zonal middleware proves about vehicle software, this
directory proves about multi-model engineering economics.

Every non-trivial decision in this repo was made by one of two routes, and
the route is recorded:

1. **Multi-model debate/plan/confer** (Crosscheck `xc`), for decisions with
   real uncertainty or cost — architecture, transport selection, QoS
   strategy, fault-scenario design. Logged in [`ledger.jsonl`](ledger.jsonl)
   with tokens, cost, wall-clock time, and dissent preserved verbatim.
2. **Single-pass implementation** (one model, no debate), for boilerplate,
   scaffolding, and mechanical work where multi-model review would burn
   tokens without improving the outcome. Also logged, at near-zero cost, so
   the ledger shows the *shape* of the cost curve — not just the expensive
   entries.

## Layout

| Path | Purpose |
|---|---|
| `ledger.jsonl` | Append-only log of every Crosscheck call: task, models, tokens, cost, wall time. The raw evidence. |
| `MODELS.md` | The routing policy — which model tier handles which kind of task, and why. |
| `adr/` | Architecture Decision Records. Each one that involved a multi-model debate preserves the dissenting positions and why they lost, not just the winning answer. |
| `task-specs/` | Versioned task envelopes handed to Crosscheck for non-trivial work (goal, constraints, acceptance criteria, budget). |
| `reports/` | Generated summaries: cost-by-phase, provenance heatmap, baseline-vs-routed cost comparison. Rendered into the dashboard's "Built with Crosscheck" tab in M6/M8. |

## Honesty rules

- A **measured baseline** (2–3 representative tasks replayed through a single
  premium model, same inputs/acceptance criteria) is required before any
  "tokens saved" claim is published. Everything else is a labeled
  **modeled counterfactual**, never presented as measured.
- Every claim of the form "Crosscheck did X" links to a `ledger.jsonl` entry
  or a `git log` commit trailer — no unverifiable assertions.
- The human is the arbiter and product owner throughout. Nothing here claims
  zero human involvement.
