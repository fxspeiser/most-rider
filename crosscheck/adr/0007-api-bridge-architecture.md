# ADR-0007: API bridge split — DDS stays in C++, Python is a file+subprocess consumer

- **Status:** accepted
- **Date:** 2026-07-28
- **Route:** single model, bounded implementation, with a real platform-compatibility spike that changed the plan mid-flight
- **Ledger ref:** `crosscheck/ledger.jsonl` entry `2026-07-28T-m5-api-bridge`

## Context

ADR-0001 named "Python + FastAPI" for the control plane/API layer, on the
assumption Python could subscribe to DDS directly. M5 needed exactly that:
something bridging DDS topics to REST/WebSocket. Before building the whole
bridge as a DDS-native Python process, a quick spike checked whether the
official `cyclonedds` PyPI package actually installs here.

## What the spike found

`pip install cyclonedds` fails on our build platform. PyPI's `cyclonedds`
11.0.1 publishes `manylinux2014_x86_64` wheels for Linux — **no aarch64
Linux wheel exists**. Our containers build as `linux/arm64` (Apple Silicon
host via Docker Desktop), so pip falls back to building from source, which
needs the local Cyclone DDS C library discoverable in a very specific
layout; several attempts (`CYCLONEDDS_HOME`, `CMAKE_PREFIX_PATH` pointed at
the exact Debian multiarch path, `--no-build-isolation`) all failed at the
same "Could not locate cyclonedds" metadata-generation step. This is a
real, confirmed platform gap, not a configuration mistake — logged here
rather than silently worked around.

## Decision

Split the bridge in two, rather than force one Python process to do both
jobs:

- **`telemetry-bridge` (C++)** — a DDS participant, same pattern as every
  other zone/service binary, subscribing to `TopologyState`,
  `DiagnosticEvent`, `PropulsionState`, `EnergyState`, `BodyState`. Writes
  an atomic JSON snapshot to a shared volume, reusing the exact
  write-then-rename pattern already used elsewhere in this codebase.
- **`api-bridge` (Python + FastAPI)** — reads that snapshot file for REST
  responses and WebSocket pushes, and separately shells out to
  `docker compose` / `tc` (via the Docker socket) for fault-control
  endpoints. It never touches DDS directly.

This keeps the language boundary ADR-0001 already established (C++ for
anything DDS-facing, Python for control plane) intact — it just moves the
DDS-facing half of "the API bridge" into the existing C++ pattern instead
of introducing a new, currently-broken dependency.

## Options considered and rejected

- **Build `cyclonedds` from source with a custom CMake toolchain file**
  matching Debian's multiarch paths exactly. Rejected: real engineering
  time to get right, for a dependency the project doesn't otherwise need,
  when the file-handoff pattern is both simpler and already proven
  elsewhere in this repo.
- **Force `--platform linux/amd64`** so a manylinux wheel installs under
  QEMU emulation. Rejected: the API/control-plane process isn't
  latency-critical (ADR-0001), so emulation slowdown wouldn't hurt
  correctness, but it adds a second, slower build path and a real
  reliability risk (QEMU-emulated Python C-extension behavior is a known
  source of flaky builds) for a demo that should be fast and reliable.

## Consequences

- Adding a new topic to the dashboard/API means updating
  `telemetry-bridge`'s subscriptions, not `api-bridge` — the Python layer
  never needs IDL knowledge.
- If a future platform (e.g., CI running on `linux/amd64`) makes the
  official wheel available, this split can be revisited — it is not a
  permanent constraint, just the honest state of the ecosystem today.
- Fault-control endpoints (`POST /api/faults/...`) require mounting the
  Docker socket into `api-bridge` — a real, disclosed security scope
  narrowing to local-demo-only use. See `docs/architecture/security-limitations.md`.
