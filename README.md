# most-rider

**Crosscheck — multi-model AI orchestration — built this zonal automotive
middleware prototype, and the repository itself is the evidence for both
claims: that the system works, and what building it actually cost.**

![Live fault-recovery demo: killing and restarting a zone on the dashboard](docs/demo/hero-demo.gif)

*The demo above: `docker compose stop rear-zone` → central's discovery
module marks it stale within ~1s → the dashboard's zone card, diagnostics
log, and fault-control panel all react live over WebSocket → restart →
recovery. No editing, this is the actual UI.*

## Quickstart — one command

```bash
docker compose up --build
open http://localhost:8282
```

That starts the full zonal middleware (four DDS zones, a discovery
module, two vehicle-domain services, a fault-injection API) and a live
dashboard on **http://localhost:8282** — real-time telemetry, zone health,
a diagnostics log, and one-click buttons for all three fault scenarios
shown above. `open http://localhost:8282/docs` for interactive OpenAPI
docs; `python3 tools/mostrider_cli.py zones` for the same data via CLI.

Full command reference (build/test/benchmark scripts, the CLI, direct API
calls) is in [Quickstart reference](#quickstart-reference) below — this
section is deliberately just the one command that matters first.

## What this is

RV Tech (Rivian + Volkswagen) is building a shared SDV electrical
architecture: zonal controllers instead of domain ECUs, a service-oriented
middleware layer, and the cloud/connectivity stack to go with it. This
project prototypes that shift — front/rear/cabin/central zones as
independent processes talking over DDS, with a powertrain/energy service,
fault injection, and measured latency/reliability under failure — using
only free, open-source building blocks.

It is a portfolio/demo artifact, not production automotive software, and
says so on purpose, repeatedly, throughout. Every performance and
reliability claim here is scoped, measured, and reproducible — see
[Honesty rules](crosscheck/README.md#honesty-rules) and
[Limitations & productionization](#limitations--productionization) below.

## Architecture

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

Key decisions, each with a linked ADR carrying the real dissent/rationale,
not just the winning answer:

- **Transport:** Eclipse Cyclone DDS, C API, C++20 zone runtime. Why, and
  what was rejected: [ADR-0001](crosscheck/adr/0001-transport-selection.md).
- **Docker networking:** explicit unicast peers, not multicast — the #1
  technical risk every model in the design debate flagged, resolved on
  day one: [ADR-0002](crosscheck/adr/0002-docker-networking-for-dds-discovery.md).
- **One zone binary, not one per zone:** `zone-runtime` is instantiated
  three times via config; only `central` is architecturally distinct — it
  hosts the discovery module rather than being "one more zone"
  ([ADR-0004](crosscheck/adr/0004-discovery-data-model.md)).
- **Priority stack: 2 of 4 layers built, 2 deferred as stretch** —
  app-level traffic classes + DDS RELIABLE/deadline/liveliness/
  transport_priority QoS on `PropulsionState`, A/B-verified against a
  best-effort flood. [ADR-0006](crosscheck/adr/0006-priority-stack-and-fault-scenarios.md).
- **Fault scenarios use real mechanisms, not app-level fakes** — `tc netem`
  for network delay, `docker compose stop`/`start` for zone loss, an actual
  flood process for congestion, all triggerable via API/CLI/dashboard. See
  [`scenarios/README.md`](scenarios/README.md).
- **API bridge is split across two languages, deliberately** —
  `telemetry-bridge` (C++) is the only DDS participant; `api-bridge`
  (Python/FastAPI) reads its JSON snapshot and never touches DDS, because
  the official `cyclonedds` PyPI package has no `linux/arm64` wheel — a
  real platform gap found by spiking it, not assumed.
  [ADR-0007](crosscheck/adr/0007-api-bridge-architecture.md).
- **Dashboard is React+Vite+TS, served same-origin from `api-bridge`** —
  no CORS, no second port. uPlot for real-time streaming charts, hand-built
  bars for the summary panel (per the dataviz skill's actual method, not a
  reflexive charting-library pick). [ADR-0008](crosscheck/adr/0008-dashboard-architecture.md).
- **Two native-vs-Docker comparisons, isolating different variables** —
  a single-container run (same OS/Docker daemon, just no bridge network)
  and a macOS-native run (disclosed as OS-confounded). CPU/memory captured
  via `docker stats`, no new instrumentation.
  [ADR-0009](crosscheck/adr/0009-benchmark-matrix-scope.md).
- **Contracts:** [`interfaces/idl/heartbeat.idl`](interfaces/idl/heartbeat.idl),
  [`interfaces/idl/discovery.idl`](interfaces/idl/discovery.idl),
  [`interfaces/idl/vehicle.idl`](interfaces/idl/vehicle.idl) — frozen and
  code-generated at build time, nothing hand-written against them is
  committed.

## Reproducible results

Every number below links to a committed, regenerable report — see
[`benchmarks/methodology.md`](benchmarks/methodology.md) for what's
measured, what isn't, and the full validity boundary.

| Comparison | Result | Report |
|---|---|---|
| Baseline latency (docker-compose) | p50 105-123us, p99 241-914us across two independent runs | [`golden-run-1`](benchmarks/reports/golden-run-1.md), [`golden-run-2`](benchmarks/reports/golden-run-2.md) |
| Bridge-network overhead (isolated) | p50 ~73-75us single-container vs. ~114-123us docker-compose — same OS, same daemon | [`linux-single-container`](benchmarks/reports/linux-single-container.md) |
| Native (OS-confounded, disclosed) | p50 ~139-158us on bare macOS, no containers at all | [`native-macos-arm64`](benchmarks/reports/native-macos-arm64.md) |
| Congestion-with-priority-survival | p99 454.8us (priority QoS on) vs. 839.9us (off) under an identical flood — far-tail caveat disclosed, not hidden | [`congestion-survival-summary`](benchmarks/reports/congestion-survival-summary.md) |
| Stale/delayed sensor (real `tc netem`) | injected 300ms delay measured as +304,548us — within 0.1% of the injected value — recovered within 2s of clearing it | [`stale-sensor-summary`](benchmarks/reports/stale-sensor-summary.md) |

A non-technical summary of all of the above — including the disclosed
open question about `central`'s CPU usage — is in
[`benchmarks/EXECUTIVE_SUMMARY.md`](benchmarks/EXECUTIVE_SUMMARY.md).

## How this was built — the Crosscheck story

**Total metered Crosscheck spend across all 8 milestones: $5.82, 628,775
tokens, 4 tool calls** — 90% of it in one planning session before any
implementation code existed. That session's output (the transport choice,
the Docker-networking risk solved on day one, "exactly three fault
scenarios," the priority stack's layer scope) held for the entire project
with zero rework. The other metered call — delegating a document to a
cheaper model — is arguably the more interesting result: it cost more than
"cheap" implies, and its own review layer correctly caught and rejected a
defective draft before it shipped.

Full accounting, including what this project explicitly *cannot* claim
(no metered cost for the single-model implementation work; no measured
single-premium-model baseline to compare against) is in
[**`crosscheck/reports/case-study.md`**](crosscheck/reports/case-study.md)
— read that before quoting a savings percentage this project doesn't
claim.

Every architectural decision traces to a named ADR under
[`crosscheck/adr/`](crosscheck/adr/); every ADR traces to either a specific
multi-model debate (dissent preserved verbatim) or an explicit single-model
routing note in [`crosscheck/MODELS.md`](crosscheck/MODELS.md). The raw
evidence behind every cost/token claim is
[`crosscheck/ledger.jsonl`](crosscheck/ledger.jsonl).

## Service catalog

| Service | Summary | Zone or service? |
|---|---|---|
| Discovery module (in `central`) | Zone health tracking, topology, diagnostics | Hosted in the `central` zone |
| [Propulsion](services/propulsion/README.md) | Torque/speed drive-cycle signal | Service (`energy-service`) |
| [Energy](services/energy/README.md) | Battery SoC, power draw, range | Service (`energy-service`) |
| [Body](services/body/README.md) | Door/headlight demo beat | Service (`body-service`) |
| [Diagnostics](services/diagnostics/README.md) | UDS-inspired event log via the API | Service (`api-bridge` + `central`) |

Each links to a full executive summary: purpose, inputs/outputs, QoS/SLA,
fault behavior, security assumptions, and demo scenarios.

## Limitations & productionization

Every gap below was deferred deliberately, each with a linked ADR
explaining why — consolidated in one place at
[**`docs/PRODUCTIONIZATION.md`**](docs/PRODUCTIONIZATION.md): security
(DDS has no auth, `api-bridge` needs Docker socket access), the 2
undelivered priority-stack layers, protocol breadth not attempted
(SOME/IP, ROS 2), measurement rigor gaps (single-host only, no payload
sweep), the Crosscheck cost story's own unmeasured pieces, and the blanket
fact that nothing here is safety-qualified.

[`docs/architecture/security-limitations.md`](docs/architecture/security-limitations.md)
covers the security-specific detail in full before you consider running
any of this beyond a local, trusted network.

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
| M7 | ✅ done | Native-vs-Docker matrix (2 variants), CPU/memory capture, cheap-model delegation experiment (correctly rejected by review) |
| M8 | ✅ done | Crosscheck case study, productionization roadmap, hero demo, pitch packaging |

## Quickstart reference

Everything below is optional depth beyond the one-command quickstart above.

```bash
# Kill a zone mid-run to see discovery react live — via compose or API:
docker compose stop rear-zone && docker compose start rear-zone
python3 tools/mostrider_cli.py fault stop-zone rear-zone
python3 tools/mostrider_cli.py fault start-zone rear-zone

# Non-interactive validation (what CI runs):
./tools/smoke-test.sh              # M0-M2: discovery, kill/restart
./tools/verify_m3_services.sh      # M3: drive-cycle correctness, cross-checked in Python

# The M4 fault scenarios, standalone (1-3 minutes each):
./tools/run_scenario_stale_sensor.sh    # real tc netem delay, measured and recovered
./tools/run_scenario_congestion.sh      # priority QoS A/B under a sensor-burst flood

# Or trigger the same faults ad hoc via API/CLI once the stack is up:
python3 tools/mostrider_cli.py fault inject-delay rear-zone 300
python3 tools/mostrider_cli.py fault clear-delay rear-zone
python3 tools/mostrider_cli.py fault congestion start
python3 tools/mostrider_cli.py fault congestion stop

# Reproduce the benchmark matrix (M7):
./tools/run_golden_benchmark.sh golden-run-3                    # docker-compose baseline (+ CPU/memory)
./tools/run_linux_process_benchmark.sh linux-single-container-2 # isolates bridge-network overhead
./tools/run_native_benchmark.sh native-macos-arm64-2            # native — OS-confounded, disclosed as such
```

**Security note:** `api-bridge`'s fault-control endpoints need Docker
socket access to work — read
[`docs/architecture/security-limitations.md`](docs/architecture/security-limitations.md)
before considering exposing port 8282 beyond localhost.

## Design system

[`design/tokens.css`](design/tokens.css) — RV-inspired palette (no
logos/wordmarks), validated for colorblind-safety and contrast against both
light and dark surfaces. Rationale and validator output in
[`design/colors.md`](design/colors.md).

## License

Proprietary — see [`LICENSE`](LICENSE). Third-party open-source components
(Eclipse Cyclone DDS, Apache-2.0, and others as they're added) remain under
their own licenses.
