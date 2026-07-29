# Crosscheck case study: what building most-rider actually cost

This is the pitch document `crosscheck/README.md` promised from day one: not
a claim, an audit trail. Every number below is pulled directly from
[`crosscheck/ledger.jsonl`](../ledger.jsonl) — recomputed for this report,
not hand-typed. If a number here disagrees with the ledger, the ledger
wins; regenerate this table from it before trusting a stale copy.

## The headline

**Total metered Crosscheck spend across all 8 milestones: $5.82, 628,775
tokens, across 4 tool calls.**

| Call | Tool | Tokens | Cost |
|---|---|---|---|
| Flight-plan `confer --super` (5 providers, single-shot) | `confer` | 23,625 | $0.4744 |
| Flight-plan `plan --thorough --super` (5-round debate) | `plan` | 423,348 | $3.5144 |
| Flight-plan `debate --thorough --super` (3-round debate) | `debate` | 108,199 | $1.2499 |
| M7 executive-summary delegation | `create_cheap` | 73,603 | $0.5833 |
| **Total** | | **628,775** | **$5.8220** |

Everything else — the entire zonal middleware, the discovery module, three
fault scenarios each independently verified, a priority QoS stack, a REST/
WebSocket/OpenAPI surface, a React dashboard, and a native-vs-Docker
benchmark matrix — was built without a single additional Crosscheck call.
Nine milestone-level ledger entries (`M0` scaffolding through `M7`'s
native/resource benchmarks) are logged as `"tool": "none"`, single-pass,
primary session — see [What this doesn't measure](#what-this-doesnt-measure)
below for why that's a disclosed limitation, not a hidden one.

## Where the $5.24 planning spend actually went

90% of all Crosscheck spend happened in one sitting, before a single line
of implementation code existed — and it is the reason almost nothing had
to be re-decided across the eight milestones that followed:

- **The transport decision** (Cyclone DDS over Fast DDS/vsomeip/ROS 2,
  [ADR-0001](../adr/0001-transport-selection.md)) was made once, correctly,
  and never revisited.
- **The #1 technical risk every provider flagged independently** — DDS
  multicast discovery breaking across Docker's bridge network — was
  identified *before* M0 wrote a single container config, and solved on
  day one ([ADR-0002](../adr/0002-docker-networking-for-dds-discovery.md))
  rather than discovered the hard way mid-build.
- **"Exactly three fault scenarios, no general chaos framework"** — a
  scope call that held for the entire project. M4 shipped precisely those
  three, no more, no scope creep.
- **The four-layer priority stack, with two layers explicitly deferred as
  stretch from the start** ([ADR-0006](../adr/0006-priority-stack-and-fault-scenarios.md))
  — M4 built exactly layers 1-2 and never had to walk back an
  over-ambitious layer 3/4 attempt.

None of this was free — $5.24 is a real number — but it bought eight
milestones of architecture that didn't need to be redone, not a slide
promising it would.

## The $0.58 that caught a real failure (M7)

The one other metered call is arguably the more interesting one:
delegating the M7 executive summary to `create_cheap` cost $0.58 for one
page — considerably more than "cheap" suggests, since the pipeline ran a
full 7-provider panel twice (scope and review) around a 7-node DAG, not a
single inexpensive model call. Its own review layer caught a real defect
(a DAG step that silently failed to read its attached source documents)
and correctly refused to ship the result. See
[ADR-0009](../adr/0009-benchmark-matrix-scope.md) for the full account.
That's the finding worth remembering: the safety net worked, on a real
failure, not a staged one.

## What this doesn't measure

Being direct about the gap matters more here than anywhere else in this
repository. Two things this case study cannot honestly claim:

1. **The cost of the single-model implementation work itself is not
   metered.** Every `"tool": "none"` ledger entry represents real,
   substantial engineering — a working DDS middleware, three verified
   fault scenarios, a dashboard, a benchmark matrix — done in one
   continuous session by one model, with no per-task token accounting
   built for it. We do not know, and do not claim to know, what that work
   "would have cost" under some other arrangement. Any number claiming
   otherwise would be invented, not measured — exactly what
   `crosscheck/README.md`'s honesty rules exist to prevent.
2. **No baseline replay exists.** The honesty rules require a *measured*
   baseline (the same representative task, replayed through a single
   premium model with matched inputs) before publishing a "tokens saved"
   number. That replay was not run for this project — building it would
   itself cost real money for a comparison this report chooses not to
   fabricate a substitute for. If this project continues, that is the
   next thing to build before any efficiency-percentage claim is made
   publicly, not after.

## What the ledger *does* prove

Not a cost saving — an audit trail. Every architectural decision in this
repository traces to a named ADR; every ADR traces to either a specific
multi-model debate (with dissent preserved verbatim, not paraphrased into
false consensus) or an explicit note that it was a single-model execution
call and why that tier was the right one
([`crosscheck/MODELS.md`](../MODELS.md)). A reviewer can check any claim
this project makes against the ledger entry that produced it. That
traceability — not a percentage — is the actual deliverable.
