// Mirrors telemetry-bridge's JSON snapshot exactly (apps/telemetry-bridge/src/main.cpp)
// and api-bridge's REST responses (apps/api-bridge/app/main.py). No IDL
// knowledge here on purpose — ADR-0007 keeps DDS entirely on the C++ side.

export interface ZoneStatus {
  alive: boolean;
  last_seq: number;
  last_seen_ns: number;
}

export interface DiagnosticEventItem {
  event_id: number;
  zone_id: string;
  severity: "info" | "warning" | "critical" | string;
  message: string;
}

export interface PropulsionState {
  vehicle_speed_kmh: number;
  torque_request_nm: number;
  torque_delivered_nm: number;
}

export interface EnergyState {
  battery_soc_pct: number;
  power_draw_kw: number;
  range_estimate_km: number;
}

export interface BodyState {
  door_open: boolean;
  headlights_on: boolean;
}

export interface Snapshot {
  generated_at_monotonic_ns: number;
  zones: Record<string, ZoneStatus>;
  diagnostics: DiagnosticEventItem[];
  propulsion: PropulsionState | null;
  energy: EnergyState | null;
  body: BodyState | null;
}

export const KNOWN_ZONES = ["front-zone", "rear-zone", "cabin-zone", "central"] as const;
export type KnownZone = (typeof KNOWN_ZONES)[number];
