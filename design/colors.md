# Color system

RV-inspired, not RV-branded: a warm gold (Rivian "compass" territory) and an
electric blue (Volkswagen territory) on a warm-charcoal dark theme, chosen
as a palette only — no logos, wordmarks, or trade dress. Footer disclaimer
on every generated report: *"Independent demo project. No affiliation with,
or endorsement by, Rivian or Volkswagen Group. No trademarks used."*

Built and validated per the `dataviz` skill's method, not eyeballed. Ramps
live in [`tokens.css`](tokens.css); this file is the rationale + evidence.

## Categorical: zone identity

| Slot | Role | Light | Dark |
|---|---|---|---|
| Brand accent | chrome, CTAs, hero numbers — **not** a zone | `#D9A441` | `#B8791F` |
| Zone: front | | `#2E86D6` | `#3D93E8` |
| Zone: cabin | | `#1BAF7A` | `#12946B` |
| Zone: rear | | `#B5482F` | `#DB5A36` |
| Zone: central | "the brain" — deliberately the odd one out | `#7A5FD6` | `#7A6BD1` |

Order is fixed and never cycled — a 6th zone folds into "Other" or facets
rather than generating a new hue (per the skill's non-negotiable rule).

### Validator output

```
$ node scripts/validate_palette.js "#D9A441,#2E86D6,#1BAF7A,#B5482F,#7A5FD6" \
    --mode light --surface "#faf9f6"

  [PASS] Lightness band         all 5 inside L 0.43-0.77
  [PASS] Chroma floor           all 5 >= 0.1
  [PASS] CVD separation         worst adjacent #B5482F<->#1BAF7A ΔE 12.7 (deutan) · tritan 6.7
  [PASS] Normal-vision floor    worst adjacent #1BAF7A<->#2E86D6 ΔE 20.9 (normal)
  [WARN] Contrast vs surface    below 3:1: #D9A441 (2.14), #1BAF7A (2.67)
  → ALL CHECKS PASS (contrast WARN requires relief — see below)

$ node scripts/validate_palette.js "#B8791F,#3D93E8,#12946B,#DB5A36,#7A6BD1" \
    --mode dark --surface "#15130f"

  [PASS] Lightness band         all 5 inside L 0.48-0.67
  [PASS] Chroma floor           all 5 >= 0.1
  [PASS] CVD separation         worst adjacent #DB5A36<->#12946B ΔE 8.9 (protan) · tritan 7.3
  [PASS] Normal-vision floor    worst adjacent #12946B<->#3D93E8 ΔE 19.7 (normal)
  [PASS] Contrast vs surface    all 5 >= 3:1
  → ALL CHECKS PASS
```

**Relief rule invoked:** the light-mode brand-gold and zone-cabin-green sit
below 3:1 contrast on the light surface (2.14 and 2.67). Per the skill this
is only legal with visible relief — every use of these two colors in the
dashboard carries a direct text label (zone name, legend entry) and never
relies on the color chip alone to convey identity. This is the same
trade-off the skill's own reference palette makes with three of its eight
slots — not a compromise unique to us.

## Status (fixed — never themed)

Unchanged from the skill's default: `good #0ca30c`, `warning #fab219`,
`serious #ec835a`, `critical #d03b3b`. These never double as a zone color —
a status hue is reserved and always ships with an icon + label, never color
alone (this matters here specifically: fault-scenario states in M4 use
these, and they must never be confused with a zone's identity color).

## Diverging: latency-vs-baseline

Blue (`--diverging-cool`, better than baseline) ↔ rust (`--diverging-warm`,
worse than baseline), neutral gray midpoint — reuses the front-zone /
rear-zone hues rather than inventing a sixth pair, so a benchmark chart and
a topology chart read as the same visual language.

## Surfaces

| | Light | Dark |
|---|---|---|
| Chart surface | `#faf9f6` | `#15130f` |
| Page plane | `#f5f3ee` | `#0e0c09` |

Dark is the primary intended theme for the live demo (matches every
provider's independent recommendation in `crosscheck/ledger.jsonl`); light
is supported for docs/reports/screenshots, not optimized as the hero view.

## What's deferred to M6

Full sequential ramps (for heatmaps/choropleths) and the texture-fill
accessibility layer are not built yet — no chart exists to consume them.
Derive them from `--zone-front` (blue) as the default sequential hue,
following the skill's step-ramp method, when the dashboard's actual charts
are built.
