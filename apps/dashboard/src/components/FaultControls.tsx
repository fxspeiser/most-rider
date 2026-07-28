import { useState } from "react";
import { faultApi } from "../faultApi";

const ZONES = ["front-zone", "rear-zone", "cabin-zone"];

// Triggers the same three M4 fault scenarios as the tools/run_scenario_*.sh
// scripts and tools/mostrider_cli.py — see scenarios/README.md for what
// each one actually proves.
export function FaultControls() {
  const [busy, setBusy] = useState<string | null>(null);
  const [log, setLog] = useState("");
  const [congestionOn, setCongestionOn] = useState(false);

  async function run(actionId: string, fn: () => Promise<unknown>) {
    setBusy(actionId);
    setLog("");
    try {
      const result = await fn();
      setLog(JSON.stringify(result));
    } catch (err) {
      setLog(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(null);
    }
  }

  return (
    <div className="fault-controls">
      <div>
        <div className="zone-label">Zone kill/restart</div>
        {ZONES.map((zone) => (
          <div className="fault-row" key={zone}>
            <span className="zone-label">{zone}</span>
            <button
              className="action danger"
              disabled={busy !== null}
              onClick={() => run(`stop-${zone}`, () => faultApi.stopZone(zone))}
            >
              Stop
            </button>
            <button
              className="action"
              disabled={busy !== null}
              onClick={() => run(`start-${zone}`, () => faultApi.startZone(zone))}
            >
              Start
            </button>
          </div>
        ))}
      </div>

      <div>
        <div className="zone-label">Stale/delayed sensor (real tc netem)</div>
        <div className="fault-row">
          <span className="zone-label">rear-zone</span>
          <button
            className="action"
            disabled={busy !== null}
            onClick={() => run("inject-delay", () => faultApi.injectDelay("rear-zone", 300))}
          >
            Inject 300ms delay
          </button>
          <button
            className="action"
            disabled={busy !== null}
            onClick={() => run("clear-delay", () => faultApi.clearDelay("rear-zone"))}
          >
            Clear delay
          </button>
        </div>
      </div>

      <div>
        <div className="zone-label">Congestion flood</div>
        <div className="fault-row">
          <button
            className="action"
            disabled={busy !== null}
            onClick={() =>
              run("congestion", async () => {
                const result = congestionOn ? await faultApi.stopCongestion() : await faultApi.startCongestion();
                setCongestionOn(!congestionOn);
                return result;
              })
            }
          >
            {congestionOn ? "Stop flood" : "Start flood"}
          </button>
        </div>
      </div>

      <div className="fault-log">{busy ? `Running ${busy}…` : log}</div>
    </div>
  );
}
