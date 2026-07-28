import type { DiagnosticEventItem } from "../types";

export function DiagnosticsLog({ events }: { events: DiagnosticEventItem[] }) {
  if (events.length === 0) {
    return <p className="diagnostic-empty">No diagnostic events yet.</p>;
  }

  // Most recent first — the log is append-only on the backend
  // (middleware/health/zone_registry.hpp transitions), so reverse for display.
  const ordered = [...events].reverse();

  return (
    <div className="diagnostics-log">
      {ordered.map((e) => (
        <div key={e.event_id} className={`diagnostic-row severity-${e.severity}`}>
          <span className="diagnostic-severity">{e.severity}</span>
          <span>{e.message}</span>
        </div>
      ))}
    </div>
  );
}
