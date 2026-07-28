# Security limitations (disclosed, not discovered)

This is a local demo prototype, not production automotive or cloud
software. Every limitation below is a deliberate scope decision, tracked
here so a reviewer finds it by reading docs, not by finding it themselves.

## DDS has no authentication (ADR-0001)

Plain Cyclone DDS with no DDS-Security: any process on the demo network can
forge propulsion/body commands or flood priority traffic (CWE-306, CWE-400
— flagged unprompted by the flight-plan panel's debate, see
`crosscheck/adr/0001-transport-selection.md`). Mitigation in this repo: the
whole stack is meant to run on an isolated Docker network on a single
trusted host. Process-per-zone containerization is **not** a security
boundary — do not present it as one.

## api-bridge has Docker socket access (M5, ADR-0007)

`api-bridge`'s fault-control endpoints (`POST/DELETE /api/faults/*`) shell
out to `docker compose`, which requires mounting `/var/run/docker.sock`
into the container. **Docker socket access is equivalent to root on the
host** — a process that can talk to the Docker daemon can create a
privileged container that mounts the host filesystem, among other things.

This is acceptable **only** because:

- The API has no authentication of its own and is meant to run on
  `localhost` during a live demo, never on a shared or public network.
- Fault-control actions are allowlisted at the application layer (a fixed
  `ALLOWED_ZONES` set — `front-zone`/`rear-zone`/`cabin-zone` only; no
  arbitrary compose service name reaches a shell), not left as free-form
  input to `docker compose`.
- `docker compose exec` (used for `tc netem` injection) targets only those
  same allowlisted zones.

**Never**:

- Publish port 8282 beyond `localhost` or a fully trusted, isolated network.
- Add new fault-control endpoints without the same zone-allowlisting
  discipline.
- Treat this pattern as a template for anything beyond a local demo — a
  real deployment would put an authenticated, least-privilege orchestration
  API in front of Docker, not a socket mount.

## Diagnostics are UDS-inspired, not certified (M5)

`api-bridge`'s `/api/diagnostics` GET/DELETE endpoints are styled after
UDS (ISO 14229) ReadDTC (0x19) / ClearDTC (0x14) service semantics — same
verb *shape*, not a certified UDS stack. No AUTOSAR, no CAN, no real ECU
diagnostic session handling. See `services/` executive summaries for what
each service actually implements.

## Everything else

See the per-ADR "Consequences" sections under `crosscheck/adr/` for
narrower, scenario-specific limitations (e.g., single-host-only latency
measurement in ADR-0002, no DSCP/tc network-layer priority enforcement in
ADR-0006).
