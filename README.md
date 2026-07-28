# most-rider

A zonal-architecture vehicle middleware prototype — and, at the same time,
a working demonstration of Crosscheck orchestrating multiple LLM providers
to design and build it. Both stories are told from the same repository, on
purpose. See [`project_overview.md`](project_overview.md) for the original
brief and [`crosscheck/`](crosscheck/) for the second story's evidence.

> **Status: M6 complete — there's a live dashboard now.** All four zones
> discover each other over Eclipse Cyclone DDS, the M4 fault scenarios all
> pass (zone kill/restart, a real `tc netem` stale-sensor delay, congestion-
> with-priority-survival), and **`http://localhost:8282`** now serves a
> live React dashboard — real-time speed/torque/SoC/power charts, zone
> health, a diagnostics log, one-click fault injection for all three M4
> scenarios, and a benchmark summary panel reading real committed report
> data — alongside the REST/WebSocket/OpenAPI surface from M5 (`/docs`,
> `/api/*`) and the CLI (`tools/mostrider_cli.py`). See
> [Quickstart](#quickstart).

## What this is

RV Tech (Rivian + Volkswagen) is building a shared SDV electrical
architecture: zonal controllers instead of domain ECUs, a service-oriented
middleware layer, and the cloud/connectivity stack to go with it. This
project prototypes that shift — front/rear/cabin/central zones as
independent processes talking over DDS, with a powertrain/energy service,
fault injection, and measured latency/reliability under failure — using
only free, open-source building blocks.

It is a portfolio/demo artifact, not production automotive software. Every
performance and reliability claim here is scoped, measured, and
reproducible — see [Honesty rules](crosscheck/README.md#honesty-rules).

## Quickstart

```bash
docker compose up --build
```

Four containers start: `central` (the discovery hub) and three peripheral
zones — `front-zone`, `rear-zone`, `cabin-zone` — all running the same
`zone-runtime` image, configured only by `ZONE_ID`/`CAPABILITIES`
environment variables (ADR-0001, ADR-0004). `central`'s logs show received
heartbeats with one-way latency, capability announcements, and
`[info]`/`[warning]` transitions whenever a zone goes stale or recovers.
Tear down with `docker compose down`.

Two more containers, `energy-service` and `body-service`, run the M3
vehicle-domain simulation independently of the zone topology above — watch
`docker compose logs -f energy-service` for a live drive cycle (speed,
torque, battery SoC, regen braking) or `body-service` for the door/light
demo beat.

`telemetry-bridge` and `api-bridge` (M5) put all of that behind a REST/
WebSocket API — and as of M6, a live dashboard — on **http://localhost:8282**:

```bash
open http://localhost:8282             # the dashboard itself
open http://localhost:8282/docs        # interactive OpenAPI docs
curl http://localhost:8282/api/zones
curl http://localhost:8282/api/propulsion
python3 tools/mostrider_cli.py zones   # same data via the CLI
```

The dashboard shows live zone health, real-time speed/torque/battery/power
charts, a diagnostics log, and buttons to trigger each M4 fault scenario —
watch the zone cards, charts, and diagnostics log react in real time when
you click one.

Try killing a zone mid-run to see discovery react live — via `docker
compose` directly, or via the API (same effect, either way):

```bash
docker compose stop rear-zone   # central logs "[warning] rear-zone went stale" within ~1s
docker compose start rear-zone  # central logs "[info] rear-zone recovered"

# or, via the API:
python3 tools/mostrider_cli.py fault stop-zone rear-zone
python3 tools/mostrider_cli.py fault start-zone rear-zone
```

**Security note:** `api-bridge`'s fault-control endpoints need Docker
socket access to work — read
[`docs/architecture/security-limitations.md`](docs/architecture/security-limitations.md)
before considering exposing port 8282 beyond localhost.

To validate the whole thing non-interactively (this is what CI runs):

```bash
./tools/smoke-test.sh              # M0-M2: discovery, kill/restart
./tools/verify_m3_services.sh      # M3: drive-cycle correctness, cross-checked in Python
```

To run the M4 fault scenarios yourself (each takes 1-3 minutes):

```bash
./tools/run_scenario_stale_sensor.sh    # real tc netem delay, measured and recovered
./tools/run_scenario_congestion.sh      # priority QoS A/B under a sensor-burst flood
```

Or trigger the same faults ad hoc via the API/CLI once the stack is up:

```bash
python3 tools/mostrider_cli.py fault inject-delay rear-zone 300
python3 tools/mostrider_cli.py fault clear-delay rear-zone
python3 tools/mostrider_cli.py fault congestion start
python3 tools/mostrider_cli.py fault congestion stop
```

See [`scenarios/README.md`](scenarios/README.md) for what each proves and
why it's built the way it is.

To reproduce the golden-run benchmark report:

```bash
./tools/run_golden_benchmark.sh golden-run-2
```

See [`benchmarks/methodology.md`](benchmarks/methodology.md) for what's
measured, what isn't, and the validity boundary (single-host only, for now).

## Architecture (M6 snapshot)

```
front-zone  --\
rear-zone   ---+--HeartBeat + CapabilityAnnounce (DDS)-->  central
cabin-zone  --/                                              |
                                                    discovery module
                                                    (ZoneRegistry: app-level
                                                     staleness tracking)
                                                              |
                                              TopologyState + DiagnosticEvent
                                                    (republished, keyed per zone)
                                                              |
energy-service --PropulsionState(priority QoS)+EnergyState-->|
body-service   --BodyState---------------------------------->+-- telemetry-bridge (C++, DDS)
                                                              |        |
load-generator --SensorBurst (congestion profile only)------>/   snapshot.json
                                                                       |
                                                              api-bridge (Python, FastAPI)
                                                                       |
                                                     REST + WebSocket + OpenAPI :8282
```

`energy-service`/`body-service`/`load-generator`/`propulsion-monitor`/
`telemetry-bridge` are services, not zones — no heartbeat, not tracked by
discovery (ADR-0005). `load-generator`/`propulsion-monitor` only run for
the congestion scenario (Compose profile `congestion`).

- **Dashboard is React+Vite+TS, served same-origin from `api-bridge`** —
  no CORS, no second port. uPlot for real-time streaming charts, hand-built
  bars for the summary panel (per the dataviz skill's actual method, not a
  reflexive charting-library pick). [ADR-0008](crosscheck/adr/0008-dashboard-architecture.md).
- **API bridge is split across two languages, deliberately** —
  `telemetry-bridge` (C++) is the only DDS participant; `api-bridge`
  (Python/FastAPI) reads its JSON snapshot and never touches DDS. Why:
  the official `cyclonedds` PyPI package has no `linux/arm64` wheel — a
  real platform gap found by spiking it, not assumed.
  [ADR-0007](crosscheck/adr/0007-api-bridge-architecture.md).
- **Fault-control endpoints need Docker socket access** — read
  [`docs/architecture/security-limitations.md`](docs/architecture/security-limitations.md)
  before exposing port 8282 beyond localhost.
- **Priority stack: 2 of 4 layers built, 2 deferred as stretch** —
  app-level traffic classes + DDS RELIABLE/deadline/liveliness/
  transport_priority QoS on `PropulsionState`, A/B-verified against a
  best-effort flood. DSCP+`tc` network-layer enforcement is not built.
  Full scope and the honest caveat about far-tail latency:
  [ADR-0006](crosscheck/adr/0006-priority-stack-and-fault-scenarios.md).
- **Fault scenarios use real mechanisms, not app-level fakes** — `tc netem`
  for network delay, `docker compose stop`/`start` for zone loss, an actual
  flood process for congestion, all now triggerable via API/CLI too. See
  [`scenarios/README.md`](scenarios/README.md).

- **Transport:** Eclipse Cyclone DDS, C API, C++20 zone runtime.
  Why, and what was rejected: [ADR-0001](crosscheck/adr/0001-transport-selection.md).
- **Docker networking:** explicit unicast peers, not multicast — the #1
  technical risk every model in the design debate flagged, resolved on
  day one: [ADR-0002](crosscheck/adr/0002-docker-networking-for-dds-discovery.md).
- **One zone binary, not one per zone:** `zone-runtime` is instantiated
  three times via config; only `central` is architecturally distinct — it
  hosts the discovery module rather than being "one more zone"
  ([ADR-0004](crosscheck/adr/0004-discovery-data-model.md)).
- **Contracts:** [`interfaces/idl/heartbeat.idl`](interfaces/idl/heartbeat.idl),
  [`interfaces/idl/discovery.idl`](interfaces/idl/discovery.idl),
  [`interfaces/idl/vehicle.idl`](interfaces/idl/vehicle.idl) — frozen and
  code-generated at build time, nothing hand-written against them is
  committed.
- **Liveliness is app-level for now** (`middleware/health/zone_registry.hpp`),
  not DDS liveliness QoS — deliberately deferred to M4, where it's one layer
  of the priority stack, not duplicated here (ADR-0004).
- **The drive cycle is analytic, not integrated** — speed is a direct
  formula, torque is its exact derivative, so it's exactly reproducible and
  independently re-derivable for verification. Why, and what's tuned vs.
  physically meaningful: [ADR-0005](crosscheck/adr/0005-drive-cycle-model.md).
  Per-service executive summaries: [`services/propulsion/`](services/propulsion/README.md),
  [`services/energy/`](services/energy/README.md), [`services/body/`](services/body/README.md),
  [`services/diagnostics/`](services/diagnostics/README.md).

## Roadmap

| Milestone | Status | Delivers |
|---|---|---|
| M0 | ✅ done | Repo skeleton, Crosscheck provenance layer, 2-zone DDS walking skeleton |
| M1 | ✅ done | Raw-sample latency pipeline, benchmark methodology + honesty rules, Golden Run #1 committed |
| M2 | ✅ done | Full front/rear/cabin/central topology, discovery module (ZoneRegistry), TopologyState/DiagnosticEvent, kill/restart verified |
| M3 | ✅ done | energy-service (propulsion+energy drive cycle) and body-service, cross-language verified, executive summaries |
| M4 | ✅ done | Priority stack (layers 1-2) + all 3 fault scenarios, each independently verified and reported |
| M5 | ✅ done | telemetry-bridge + api-bridge: REST/WebSocket/OpenAPI on :8282, CLI, mini-diagnostics, API-triggerable fault injection |
| M6 | ✅ done | React dashboard on :8282 — real-time charts, zone health, diagnostics log, one-click fault injection, benchmark summary |
| M7 | next | Honest, reproducible benchmarks |
| M8 | planned | Crosscheck case study + pitch packaging |

Full milestone detail and what's explicitly cut from v1 (SOME/IP, ROS 2,
Kubernetes, DDS-Security, ...) lives in the flight-plan ADRs under
[`crosscheck/adr/`](crosscheck/adr/).

## The Crosscheck story

Every non-trivial decision in this repo is routed to one of a few tiers —
full multi-model debate for genuinely uncertain architecture calls, a
single model for bounded implementation, a cheap model for boilerplate —
and logged. See [`crosscheck/README.md`](crosscheck/README.md) for the full
provenance layer, [`crosscheck/MODELS.md`](crosscheck/MODELS.md) for the
routing policy, and [`crosscheck/ledger.jsonl`](crosscheck/ledger.jsonl) for
the raw token/cost/time evidence behind every claim this README makes.

## Design system

[`design/tokens.css`](design/tokens.css) — RV-inspired palette (no
logos/wordmarks), validated for colorblind-safety and contrast against both
light and dark surfaces. Rationale and validator output in
[`design/colors.md`](design/colors.md).

## License

Proprietary — see [`LICENSE`](LICENSE). Third-party open-source components
(Eclipse Cyclone DDS, Apache-2.0, and others as they're added) remain under
their own licenses.
