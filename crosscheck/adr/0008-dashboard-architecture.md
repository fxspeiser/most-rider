# ADR-0008: Dashboard stack — React+Vite+TS, uPlot for real-time, hand-built bars for summary

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** single model, bounded implementation — executing the flight plan's named dashboard deliverable, refined against the dataviz skill's actual methodology once loaded
- **Ledger ref:** `crosscheck/ledger.jsonl` entry `2026-07-28T-m6-dashboard`

## Context

The original flight-plan panel (see `crosscheck/ledger.jsonl`'s M0 planning
entries) proposed "React/Vite + uPlot (real-time) + Recharts/ECharts
(summary/report views)." Building M6, the `dataviz` skill was loaded (as
its trigger conditions require for any chart work) — its actual method is
hand-built HTML/SVG components against mark specs and a validated palette,
not "pick a charting library." That's a refinement worth recording, not a
silent deviation.

## Decision 1: React + Vite + TypeScript, served same-origin from api-bridge

`apps/dashboard` is a small Vite+React+TS app. Built in its own Docker
stage and copied into `api-bridge`'s image as static files (`STATIC_DIR`,
mounted via FastAPI's `StaticFiles`) — same origin as the REST/WebSocket
API on port 8282, so there's no CORS configuration and no second port to
document or secure.

## Decision 2: uPlot for real-time, hand-built bars for summary — not one library for both

- **Real-time charts** (speed/torque, SoC/power): `uPlot`, matching the
  flight plan's original pick — purpose-built for streaming line charts,
  tiny (no React wrapper library needed, ~50 lines of direct API usage).
- **Summary panel** (benchmark p99 comparison): hand-built `div`-based
  horizontal bars, not Recharts/ECharts. Per the dataviz skill's actual
  method (`references/marks-and-anatomy.md`, `references/components.md`):
  build the mark directly against the spec (thin bar, direct value label,
  single accent color since this is one metric across named runs, not
  multiple overlapping series) rather than pulling in a general-purpose
  charting library for a four-row bar comparison. Smaller bundle, and the
  code reads as "this is what a bar is," not "here's a library's opinion
  about what a bar is."

Real-time data itself is accumulated client-side (`useSnapshot.ts`'s
rolling `HistoryPoint[]` buffer) — `telemetry-bridge`'s snapshot only ever
carries the *latest* value per topic (M5, ADR-0007), so history exists only
in the browser, not on the wire.

## Decision 3: `design/tokens.css` stays the single source of truth

The dashboard needs the same validated RV-inspired palette
`design/tokens.css` already defines (M0, validated against the dataviz
skill's CVD/contrast checker). Rather than duplicate values into the
dashboard or fight Vite's dev-server file-system restrictions with an
`fs.allow` workaround, `predev`/`prebuild` npm scripts (`cp
../../design/tokens.css src/tokens.css`) generate a local copy — mirrored
in Docker by giving the build stage the same relative directory layout
(`design/` next to `apps/dashboard/`) so the identical script runs
unmodified in both environments. The generated copy is gitignored; only
the canonical `design/tokens.css` is committed.

## Bugs found by actually running this, not by review

- **uPlot resolves colors once, at chart-creation time** — canvas
  `strokeStyle` doesn't track CSS custom property changes the way DOM
  styles do. The chart never updated when the theme toggle switched
  light/dark, confirmed by screenshotting both themes and seeing the
  light-mode chart still rendering with dark-mode's resolved `--gridline`
  value (too heavy/dark for the light background). Fixed by threading
  `theme` into the chart's creation-effect dependency array, forcing a
  rebuild on toggle.
- **`ZoneCards`' "time since" label was misleading** — `TopologyState` (and
  therefore `last_seen_ns`) is only republished on an alive/stale
  *transition* (`middleware/health/zone_registry.hpp`), not on every
  heartbeat. A zone alive for 40+ minutes with no drama correctly showed a
  large "seconds ago" number, which reads as a false staleness warning.
  Caught by screenshotting a long-running stack. Relabeled to "alive for
  Xm Ys," which is what the field actually means.
- **Local-import-path vs. Docker-copy-path mismatch**, caught before it
  ever broke a build: an early version imported `tokens.css` via a
  three-`../` relative path (correct for local dev, where the full repo
  exists on disk) while the Docker stage planned to copy the file to a
  different relative location. Resolved by Decision 3's matching-layout
  approach instead of maintaining two diverging import paths.

## Consequences

- Every chart added later should default to this same pattern: uPlot for
  anything streaming/real-time, hand-built marks for anything summary/
  categorical, both validated against `design/tokens.css` and the dataviz
  skill's checks — not a reflexive reach for a charting library.
- `GET /api/reports` / `GET /api/reports/{name}` (added to `api-bridge` for
  this milestone) is now a small, reusable read surface for any future
  static-report data, not dashboard-specific.
