# most-rider

A zonal-architecture vehicle middleware prototype — and, at the same time,
a working demonstration of Crosscheck orchestrating multiple LLM providers
to design and build it. Both stories are told from the same repository, on
purpose. See [`project_overview.md`](project_overview.md) for the original
brief and [`crosscheck/`](crosscheck/) for the second story's evidence.

> **Status: M1 complete.** Two zones (`front-zone`, `central`) discover each
> other over Eclipse Cyclone DDS across a Docker bridge network and exchange
> heartbeats — no multicast, no manual steps, one command. Golden Run #1 is
> in: **p50 123us / p99 320us / p99.9 675us**, single Docker host, n=146 —
> see [`benchmarks/reports/golden-run-1.md`](benchmarks/reports/golden-run-1.md)
> for the full disclosure. See [Quickstart](#quickstart).

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

Two containers start: `central` (subscriber) and `front-zone` (publisher).
`central`'s logs show received heartbeats with one-way latency in
microseconds. Tear down with `docker compose down`.

To validate the whole thing non-interactively (this is what CI runs):

```bash
./tools/smoke-test.sh
```

To reproduce the golden-run benchmark report:

```bash
./tools/run_golden_benchmark.sh golden-run-2
```

See [`benchmarks/methodology.md`](benchmarks/methodology.md) for what's
measured, what isn't, and the validity boundary (single-host only, for now).

## Architecture (M0 snapshot)

```
front-zone  --HeartBeat (DDS)-->  central
     \                              /
      \-- Cyclone DDS, unicast peers (deploy/cyclonedds-peers.xml) --/
```

- **Transport:** Eclipse Cyclone DDS, C API, C++20 zone runtime.
  Why, and what was rejected: [ADR-0001](crosscheck/adr/0001-transport-selection.md).
- **Docker networking:** explicit unicast peers, not multicast — the #1
  technical risk every model in the design debate flagged, resolved on
  day one: [ADR-0002](crosscheck/adr/0002-docker-networking-for-dds-discovery.md).
- **Contract:** [`interfaces/idl/heartbeat.idl`](interfaces/idl/heartbeat.idl),
  frozen and code-generated at build time — nothing hand-written against it
  is committed.

## Roadmap

| Milestone | Status | Delivers |
|---|---|---|
| M0 | ✅ done | Repo skeleton, Crosscheck provenance layer, 2-zone DDS walking skeleton |
| M1 | ✅ done | Raw-sample latency pipeline, benchmark methodology + honesty rules, Golden Run #1 committed |
| M2 | next | Full front/rear/cabin/central topology, discovery module, diagnostics |
| M3 | planned | Powertrain/energy service, body control |
| M4 | planned | Priority stack + all 3 fault scenarios (incl. the congestion-survival centerpiece) |
| M5 | planned | REST/OpenAPI/WebSocket/CLI surface, mini-diagnostics |
| M6 | planned | Dashboard (real-time + summary charting) |
| M7 | planned | Honest, reproducible benchmarks |
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
