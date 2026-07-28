import { useEffect, useRef, useState } from "react";
import type { Snapshot } from "./types";

export interface HistoryPoint {
  t: number; // seconds since first sample this session
  speed_kmh: number;
  torque_nm: number;
  soc_pct: number;
  power_kw: number;
}

const HISTORY_LIMIT = 300; // ~60s at the telemetry-bridge's 200ms snapshot period

// Connects to api-bridge's /ws (same origin), keeps the latest snapshot and
// a rolling client-side history for the real-time charts — the snapshot
// itself only ever carries the *latest* value per topic (see
// apps/telemetry-bridge), so history accumulation happens here, not on the
// wire.
export function useSnapshot() {
  const [snapshot, setSnapshot] = useState<Snapshot | null>(null);
  const [connected, setConnected] = useState(false);
  const [history, setHistory] = useState<HistoryPoint[]>([]);
  const startRef = useRef<number | null>(null);

  useEffect(() => {
    let ws: WebSocket | null = null;
    let retryTimer: ReturnType<typeof setTimeout> | null = null;
    let cancelled = false;

    const connect = () => {
      if (cancelled) return;
      const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
      ws = new WebSocket(`${protocol}//${window.location.host}/ws`);

      ws.onopen = () => setConnected(true);
      ws.onclose = () => {
        setConnected(false);
        if (!cancelled) retryTimer = setTimeout(connect, 1000);
      };
      ws.onerror = () => ws?.close();

      ws.onmessage = (event) => {
        const data: Snapshot = JSON.parse(event.data);
        setSnapshot(data);

        if (data.propulsion && data.energy) {
          const nowS = data.generated_at_monotonic_ns / 1e9;
          if (startRef.current === null) startRef.current = nowS;
          const t = nowS - startRef.current;

          setHistory((prev) => {
            const next = [
              ...prev,
              {
                t,
                speed_kmh: data.propulsion!.vehicle_speed_kmh,
                torque_nm: data.propulsion!.torque_delivered_nm,
                soc_pct: data.energy!.battery_soc_pct,
                power_kw: data.energy!.power_draw_kw,
              },
            ];
            return next.length > HISTORY_LIMIT ? next.slice(next.length - HISTORY_LIMIT) : next;
          });
        }
      };
    };

    connect();
    return () => {
      cancelled = true;
      if (retryTimer) clearTimeout(retryTimer);
      ws?.close();
    };
  }, []);

  return { snapshot, connected, history };
}
