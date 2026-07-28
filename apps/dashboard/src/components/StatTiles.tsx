import type { BodyState, EnergyState, PropulsionState } from "../types";

function Tile({ label, value, accent }: { label: string; value: string; accent?: string }) {
  return (
    <div className={`stat-tile${accent ? ` accent-${accent}` : ""}`}>
      <div className="value">{value}</div>
      <div className="label">{label}</div>
    </div>
  );
}

export function PropulsionTiles({ propulsion }: { propulsion: PropulsionState | null }) {
  if (!propulsion) return <p className="loading-note">Waiting for energy-service…</p>;
  return (
    <div className="stat-row">
      <Tile label="Speed" value={`${propulsion.vehicle_speed_kmh.toFixed(1)} km/h`} accent="front" />
      <Tile label="Torque delivered" value={`${propulsion.torque_delivered_nm.toFixed(0)} Nm`} />
      <Tile label="Torque requested" value={`${propulsion.torque_request_nm.toFixed(0)} Nm`} />
    </div>
  );
}

export function EnergyTiles({ energy }: { energy: EnergyState | null }) {
  if (!energy) return <p className="loading-note">Waiting for energy-service…</p>;
  const regen = energy.power_draw_kw < 0;
  return (
    <div className="stat-row">
      <Tile label="Battery SoC" value={`${energy.battery_soc_pct.toFixed(1)}%`} accent="cabin" />
      <Tile
        label={regen ? "Regen charging" : "Power draw"}
        value={`${Math.abs(energy.power_draw_kw).toFixed(1)} kW`}
      />
      <Tile label="Range estimate" value={`${energy.range_estimate_km.toFixed(0)} km`} />
    </div>
  );
}

export function BodyTiles({ body }: { body: BodyState | null }) {
  if (!body) return <p className="loading-note">Waiting for body-service…</p>;
  return (
    <div className="stat-row">
      <Tile label="Door" value={body.door_open ? "Open" : "Closed"} />
      <Tile label="Headlights" value={body.headlights_on ? "On" : "Off"} />
    </div>
  );
}
