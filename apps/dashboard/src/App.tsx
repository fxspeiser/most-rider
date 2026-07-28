import { useEffect, useState } from "react";
import { useSnapshot } from "./useSnapshot";
import { ZoneCards } from "./components/ZoneCards";
import { DiagnosticsLog } from "./components/DiagnosticsLog";
import { PropulsionTiles, EnergyTiles, BodyTiles } from "./components/StatTiles";
import { RealtimeChart } from "./components/RealtimeChart";
import { FaultControls } from "./components/FaultControls";
import { BenchmarkSummary } from "./components/BenchmarkSummary";

type Theme = "light" | "dark";

function useTheme(): [Theme, () => void] {
  const [theme, setTheme] = useState<Theme>(() => (localStorage.getItem("theme") as Theme) || "dark");

  useEffect(() => {
    document.documentElement.setAttribute("data-theme", theme);
    localStorage.setItem("theme", theme);
  }, [theme]);

  return [theme, () => setTheme((t) => (t === "dark" ? "light" : "dark"))];
}

export default function App() {
  const { snapshot, connected, history } = useSnapshot();
  const [theme, toggleTheme] = useTheme();

  return (
    <div className="viz-root">
      <header className="header">
        <div>
          <h1>most-rider</h1>
          <span className="subtitle">zonal middleware — live telemetry</span>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: 12 }}>
          <span className="connection-badge">
            <span className={`connection-dot${connected ? " connected" : ""}`} />
            {connected ? "live" : "reconnecting…"}
          </span>
          <button className="theme-toggle" onClick={toggleTheme}>
            {theme === "dark" ? "☀ light" : "● dark"}
          </button>
        </div>
      </header>

      <div className="dashboard-grid">
        <section className="panel">
          <h2>Zones (discovery module, M2)</h2>
          <ZoneCards zones={snapshot?.zones ?? {}} nowNs={snapshot?.generated_at_monotonic_ns ?? 0} />
        </section>

        <section className="panel">
          <h2>Propulsion (M3)</h2>
          <PropulsionTiles propulsion={snapshot?.propulsion ?? null} />
        </section>

        <section className="panel">
          <h2>Energy (M3)</h2>
          <EnergyTiles energy={snapshot?.energy ?? null} />
        </section>

        <section className="panel">
          <h2>Body (M3)</h2>
          <BodyTiles body={snapshot?.body ?? null} />
        </section>
      </div>

      <section className="panel">
        <h2>Speed &amp; torque (live)</h2>
        <RealtimeChart
          data={history}
          theme={theme}
          series={[
            { key: "speed_kmh", label: "Speed (km/h)", colorVar: "--zone-front" },
            { key: "torque_nm", label: "Torque delivered (Nm)", colorVar: "--zone-rear" },
          ]}
        />
      </section>

      <section className="panel">
        <h2>Battery &amp; power (live)</h2>
        <RealtimeChart
          data={history}
          theme={theme}
          series={[
            { key: "soc_pct", label: "Battery SoC (%)", colorVar: "--zone-cabin" },
            { key: "power_kw", label: "Power draw (kW)", colorVar: "--zone-central" },
          ]}
        />
      </section>

      <div className="dashboard-grid">
        <section className="panel">
          <h2>Diagnostics (M2/M5)</h2>
          <DiagnosticsLog events={snapshot?.diagnostics ?? []} />
        </section>

        <section className="panel">
          <h2>Fault scenarios (M4/M5)</h2>
          <FaultControls />
        </section>
      </div>

      <section className="panel">
        <h2>Benchmark summary (M1/M4/M7)</h2>
        <BenchmarkSummary />
      </section>
    </div>
  );
}
