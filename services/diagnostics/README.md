# Diagnostics service — executive summary

## Purpose

A lightweight, UDS-inspired (ISO 14229) event log — not a certified UDS
implementation. Gives a client one place to ask "what's gone wrong
recently" without subscribing to raw DDS topics.

## Status

Two layers, both already built:

- **Event source**: central's discovery module publishes `DiagnosticEvent`
  on every zone alive/stale transition (M2, `ADR-0004`).
- **API surface** (M5): `api-bridge` exposes this as
  `GET /api/diagnostics` (UDS-inspired ReadDTC / 0x19-style) and
  `DELETE /api/diagnostics` (UDS-inspired ClearDTC / 0x14-style).

No `ReadDataByIdentifier` (0x22) equivalent exists yet — the closest
analogues are `/api/zones`, `/api/propulsion`, `/api/energy`, `/api/body`,
which serve the same "read current data" role as separate, more specific
endpoints rather than one generic by-identifier lookup.

## Inputs / outputs

- **Reads:** `DiagnosticEvent` (via `telemetry-bridge`'s snapshot, itself
  fed by central's discovery module).
- **Exposes:** `GET /api/diagnostics` (JSON list of recent events, severity
  `info`/`warning`), `DELETE /api/diagnostics` (clears this API's view via
  an event-id watermark — the underlying DDS event log is untouched,
  matching a real UDS client's relationship to a vehicle's persistent DTC
  store, which a client-side clear doesn't erase either).

## Owner / node

`api-bridge` container (the API-facing half); `central` (the event source).

## QoS and SLA

Best-effort, no deadline — a diagnostics feed is inherently "eventually
consistent," not safety-critical.

## Fault behavior

If `telemetry-bridge`'s snapshot is unavailable, `/api/diagnostics` returns
HTTP 503, not a stale or empty 200 — callers can distinguish "no
diagnostics" from "the bridge is down."

## Security assumptions

No authentication (see `docs/architecture/security-limitations.md`).
Read-only from a system-safety perspective: clearing diagnostics via this
API cannot mask a real fault, since it only affects this API's own
watermark, not the DDS event stream or `central`'s internal state.

## API / topics

- DDS topic: `DiagnosticEvent` (`interfaces/idl/discovery.idl`).
- REST: `GET /api/diagnostics`, `DELETE /api/diagnostics` — see
  `apps/api-bridge/app/main.py` and the auto-generated OpenAPI docs at
  `/docs` on port 8282.
- CLI: `tools/mostrider_cli.py diagnostics [--clear]`.

## Demo scenarios

Run any M4 fault scenario (`scenarios/README.md`) and watch
`GET /api/diagnostics` (or `tools/mostrider_cli.py diagnostics`) pick up
the resulting `[warning] <zone> went stale`, then clear it and confirm the
list empties while the events keep flowing to any other subscriber.
