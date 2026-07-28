import { useEffect, useState } from "react";

interface ReportStats {
  count: number;
  p50_us: number;
  p99_us: number;
}
interface Report {
  run_id: string;
  stats: ReportStats;
}

// Curated, in a fixed display order — not every report in benchmarks/reports/,
// just the ones that tell the M4/M7 story concisely. Anything missing (not
// yet run) is silently skipped, not shown as a zero.
const CURATED = [
  { name: "golden-run-1", label: "Golden Run #1 (M1 baseline)" },
  { name: "congestion-baseline", label: "Congestion: baseline (no flood)" },
  { name: "congestion-no-priority", label: "Congestion: flood, no priority QoS" },
  { name: "congestion-priority", label: "Congestion: flood, priority QoS" },
];

// Single metric (p99 latency), single semantic entity per bar — a fixed
// accent color is correct here per the dataviz skill (categorical hues are
// for distinguishing multiple series in one mark, not required for a plain
// single-series comparison).
const BAR_COLOR = "var(--brand-accent)";

export function BenchmarkSummary() {
  const [reports, setReports] = useState<Record<string, Report>>({});
  const [loaded, setLoaded] = useState(false);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      const available: string[] = await fetch("/api/reports").then((r) => r.json());
      const wanted = CURATED.filter((c) => available.includes(c.name));
      const fetched = await Promise.all(
        wanted.map((c) => fetch(`/api/reports/${c.name}`).then((r) => r.json() as Promise<Report>)),
      );
      if (cancelled) return;
      const byName: Record<string, Report> = {};
      wanted.forEach((c, i) => (byName[c.name] = fetched[i]));
      setReports(byName);
      setLoaded(true);
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  const rows = CURATED.filter((c) => reports[c.name]);

  if (!loaded) return <p className="loading-note">Loading benchmark reports…</p>;
  if (rows.length === 0) {
    return (
      <p className="loading-note">
        No benchmark reports found yet — run tools/run_golden_benchmark.sh or
        tools/run_scenario_congestion.sh, then reload.
      </p>
    );
  }

  const maxP99 = Math.max(...rows.map((r) => reports[r.name].stats.p99_us));

  return (
    <div>
      <div className="summary-bars">
        {rows.map((r) => {
          const stats = reports[r.name].stats;
          const widthPct = maxP99 > 0 ? (stats.p99_us / maxP99) * 100 : 0;
          return (
            <div className="summary-bar-row" key={r.name}>
              <span>{r.label}</span>
              <div className="summary-bar-track">
                <div
                  className="summary-bar-fill"
                  style={{ width: `${widthPct}%`, background: BAR_COLOR }}
                />
              </div>
              <span className="summary-bar-value">{stats.p99_us.toFixed(0)}us</span>
            </div>
          );
        })}
      </div>
      <p className="summary-note">
        p99 latency, real committed reports from benchmarks/reports/ — see
        scenarios/README.md and benchmarks/methodology.md for what each run
        measured and its validity boundary.
      </p>
    </div>
  );
}
