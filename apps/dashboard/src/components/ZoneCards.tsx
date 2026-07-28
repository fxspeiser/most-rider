import type { ZoneStatus } from "../types";

const DISPLAY_ORDER = ["front-zone", "rear-zone", "cabin-zone", "central"];

// TopologyState is only republished on an alive/stale TRANSITION
// (middleware/health/zone_registry.hpp — evaluate() returns only zones
// whose status just flipped), not on every heartbeat. So this is "time
// since the zone's status last changed," not "time since its last
// heartbeat" — a zone that's been alive for an hour with no drama still
// shows a large number here, correctly, but a label implying freshness
// would read as a false staleness warning. Caught by screenshotting a
// long-running stack and seeing "2454.6s ago" next to a green "alive" dot.
function formatSinceTransition(lastSeenNs: number, nowNs: number): string {
  const ageS = (nowNs - lastSeenNs) / 1e9;
  if (ageS < 0 || !Number.isFinite(ageS)) return "";
  if (ageS < 1) return "just now";
  if (ageS < 60) return `${ageS.toFixed(0)}s`;
  return `${Math.floor(ageS / 60)}m ${Math.floor(ageS % 60)}s`;
}

export function ZoneCards({
  zones,
  nowNs,
}: {
  zones: Record<string, ZoneStatus>;
  // Pass the snapshot's own generated_at_monotonic_ns, not the browser's
  // clock — last_seen_ns is in telemetry-bridge's CLOCK_MONOTONIC domain,
  // which the browser doesn't share (ADR-0002 only guarantees this across
  // containers on the same host, not into a separate browser process).
  nowNs: number;
}) {
  const known = DISPLAY_ORDER.filter((z) => z in zones);
  const rest = Object.keys(zones).filter((z) => !DISPLAY_ORDER.includes(z));
  const ordered = [...known, ...rest];

  if (ordered.length === 0) {
    return <p className="loading-note">Waiting for zones to report in via central's discovery module…</p>;
  }

  return (
    <div className="zone-cards">
      {ordered.map((zoneId) => {
        const z = zones[zoneId];
        return (
          <div className="zone-card" key={zoneId}>
            <span className="zone-name">{zoneId}</span>
            <span className="zone-status">
              <span className={`zone-status-dot ${z.alive ? "alive" : "stale"}`} />
              {z.alive ? "alive" : "stale"}
            </span>
            <span className="zone-seq">
              {z.alive ? "alive" : "stale"} for {formatSinceTransition(z.last_seen_ns, nowNs)}
              {" · seq "}
              {z.last_seq}
            </span>
          </div>
        );
      })}
    </div>
  );
}
