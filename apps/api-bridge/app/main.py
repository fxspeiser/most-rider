"""api-bridge: REST/WebSocket/OpenAPI surface over telemetry-bridge's DDS
snapshot, plus fault-control endpoints for the M4 scenarios (ADR-0007).

This process never touches DDS directly — the official `cyclonedds`
package has no linux/arm64 wheel (ADR-0007's platform-compatibility
finding). It reads the JSON file telemetry-bridge writes and shells out to
`docker compose` for fault control.

SECURITY: the /api/faults/* endpoints require Docker socket access,
equivalent to root on the host. This is a local-demo-only pattern with no
authentication — see docs/architecture/security-limitations.md. Never
expose this service beyond localhost or a fully trusted network.
"""
import asyncio
import json
import os
import subprocess
import time
from pathlib import Path

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from pydantic import BaseModel

SNAPSHOT_PATH = Path(os.environ.get("SNAPSHOT_PATH", "/data/snapshot.json"))
# COMPOSE_PROJECT_DIR: where THIS container reads docker-compose.yml from
# (a path inside api-bridge's own filesystem). HOST_PROJECT_DIR: the real
# host path the Docker daemon needs to resolve bind mounts when api-bridge's
# own `docker compose` calls create new containers — see ADR-0007 and the
# docker-compose.yml comment on api-bridge's environment block. Passing
# --project-directory keeps every relative volume path in docker-compose.yml
# resolving against the host, not this container's internal view of it.
COMPOSE_PROJECT_DIR = os.environ.get("COMPOSE_PROJECT_DIR", "/workspace")
HOST_PROJECT_DIR = os.environ.get("HOST_PROJECT_DIR", COMPOSE_PROJECT_DIR)

# Allowlisted, not user-supplied: fault endpoints only ever touch these
# three peripheral zones, never an arbitrary compose service name.
ALLOWED_ZONES = {"front-zone", "rear-zone", "cabin-zone"}

app = FastAPI(
    title="most-rider API",
    description=(
        "REST/WebSocket surface over the zonal middleware's DDS telemetry "
        "(via telemetry-bridge), plus fault-control endpoints for the M4 "
        "scenarios (scenarios/README.md). Diagnostics are UDS-inspired "
        "(0x19 ReadDTC / 0x14 ClearDTC in spirit) — not a certified "
        "UDS/ISO 14229 implementation."
    ),
    version="0.5.0",
)

# Watermark for the "clear diagnostics" endpoint — see clear_diagnostics().
_diagnostics_cleared_before_event_id = -1


def read_snapshot() -> dict:
    try:
        return json.loads(SNAPSHOT_PATH.read_text())
    except FileNotFoundError as exc:
        raise HTTPException(
            status_code=503, detail="telemetry-bridge snapshot not available yet"
        ) from exc
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=503, detail="snapshot mid-write, retry") from exc


def run_compose(*args: str) -> subprocess.CompletedProcess:
    # -f: where THIS container reads the compose file from (its own
    # filesystem). --project-directory: the string sent to the host daemon
    # for resolving the file's relative bind-mount paths (ADR-0007) — the
    # two are deliberately different paths on two different filesystems.
    return subprocess.run(
        ["docker", "compose",
         "-f", f"{COMPOSE_PROJECT_DIR}/docker-compose.yml",
         "--project-directory", HOST_PROJECT_DIR,
         *args],
        capture_output=True, text=True, timeout=30,
    )


def validated_zone(zone: str) -> str:
    if zone not in ALLOWED_ZONES:
        raise HTTPException(
            status_code=400,
            detail=f"unknown zone '{zone}', must be one of {sorted(ALLOWED_ZONES)}",
        )
    return zone


@app.get("/api/health")
def health():
    age_s = time.time() - SNAPSHOT_PATH.stat().st_mtime if SNAPSHOT_PATH.exists() else None
    return {"status": "ok", "snapshot_age_s": age_s}


@app.get("/api/zones")
def get_zones():
    """Topology + liveliness for every known zone (from central's discovery
    module via TopologyState — see ADR-0004)."""
    return read_snapshot()["zones"]


@app.get("/api/diagnostics")
def get_diagnostics():
    """UDS-inspired ReadDTC (0x19-style) — not a certified UDS implementation."""
    events = read_snapshot()["diagnostics"]
    return [e for e in events if e["event_id"] > _diagnostics_cleared_before_event_id]


@app.delete("/api/diagnostics")
def clear_diagnostics():
    """UDS-inspired ClearDTC (0x14-style). Clears this API's view only —
    the underlying event log in telemetry-bridge/DDS is untouched, matching
    a real UDS client's relationship to a vehicle's persistent DTC store."""
    global _diagnostics_cleared_before_event_id
    events = read_snapshot()["diagnostics"]
    if events:
        _diagnostics_cleared_before_event_id = max(e["event_id"] for e in events)
    return {"cleared_before_event_id": _diagnostics_cleared_before_event_id}


@app.get("/api/propulsion")
def get_propulsion():
    return read_snapshot()["propulsion"]


@app.get("/api/energy")
def get_energy():
    return read_snapshot()["energy"]


@app.get("/api/body")
def get_body():
    return read_snapshot()["body"]


@app.websocket("/ws")
async def websocket_snapshot(websocket: WebSocket):
    """Pushes the full snapshot whenever telemetry-bridge updates it."""
    await websocket.accept()
    last_sent = None
    try:
        while True:
            if SNAPSHOT_PATH.exists():
                contents = SNAPSHOT_PATH.read_text()
                if contents != last_sent:
                    await websocket.send_text(contents)
                    last_sent = contents
            await asyncio.sleep(0.2)
    except WebSocketDisconnect:
        pass


@app.post("/api/faults/zones/{zone}/stop")
def stop_zone(zone: str):
    """Fault scenario 1 (zone kill/restart) — scenarios/README.md."""
    zone = validated_zone(zone)
    result = run_compose("stop", zone)
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail=result.stderr)
    return {"zone": zone, "action": "stopped"}


@app.post("/api/faults/zones/{zone}/start")
def start_zone(zone: str):
    zone = validated_zone(zone)
    result = run_compose("start", zone)
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail=result.stderr)
    return {"zone": zone, "action": "started"}


class NetworkFault(BaseModel):
    delay_ms: int = 300
    jitter_ms: int = 20


@app.post("/api/faults/network/{zone}")
def inject_network_fault(zone: str, fault: NetworkFault):
    """Fault scenario 2 (stale/delayed sensor) — real `tc netem`, not an
    app-level fake. See scenarios/README.md."""
    zone = validated_zone(zone)
    result = run_compose(
        "exec", "-T", zone, "tc", "qdisc", "add", "dev", "eth0", "root",
        "netem", "delay", f"{fault.delay_ms}ms", f"{fault.jitter_ms}ms",
    )
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail=result.stderr)
    return {"zone": zone, "delay_ms": fault.delay_ms, "jitter_ms": fault.jitter_ms}


@app.delete("/api/faults/network/{zone}")
def clear_network_fault(zone: str):
    zone = validated_zone(zone)
    result = run_compose("exec", "-T", zone, "tc", "qdisc", "del", "dev", "eth0", "root")
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail=result.stderr)
    return {"zone": zone, "action": "cleared"}


@app.post("/api/faults/congestion")
def start_congestion():
    """Fault scenario 3 (congestion-with-priority-survival) — starts the
    flood. See scenarios/README.md."""
    result = run_compose("--profile", "congestion", "up", "-d", "load-generator")
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail=result.stderr)
    return {"action": "congestion started"}


@app.delete("/api/faults/congestion")
def stop_congestion():
    result = run_compose("--profile", "congestion", "stop", "load-generator")
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail=result.stderr)
    return {"action": "congestion stopped"}
