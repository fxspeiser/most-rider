import { useEffect, useRef } from "react";
import uPlot from "uplot";
import "uplot/dist/uPlot.min.css";
import type { HistoryPoint } from "../useSnapshot";
import { resolveCssVar } from "../cssVars";

export interface SeriesSpec {
  key: keyof HistoryPoint;
  label: string;
  colorVar: string; // e.g. "--zone-front"
}

// Thin uPlot wrapper, chosen per ADR-0008 for the real-time series
// specifically — high sample rate, low overhead, matching the flight
// plan's original pick for streaming charts (summary/report charts use
// hand-built SVG bars instead, see BenchmarkSummary.tsx).
export function RealtimeChart({
  data,
  series,
  height = 180,
  theme,
}: {
  data: HistoryPoint[];
  series: SeriesSpec[];
  height?: number;
  // Forces the chart to rebuild when the theme toggles — uPlot draws to a
  // <canvas> with colors resolved once at creation time (see
  // resolveCssVar), so it never picks up a CSS variable change on its own.
  // Caught by actually screenshotting both themes: the light-mode grid
  // looked far too heavy because it was still using dark-mode's resolved
  // --gridline value.
  theme: string;
}) {
  const containerRef = useRef<HTMLDivElement>(null);
  const plotRef = useRef<uPlot | null>(null);

  useEffect(() => {
    if (!containerRef.current) return;

    const colors = series.map((s) => resolveCssVar(s.colorVar));

    const opts: uPlot.Options = {
      width: containerRef.current.clientWidth,
      height,
      legend: { show: false }, // custom legend rendered above the chart — see chart-legend in styles.css
      cursor: { show: true },
      scales: { x: { time: false } },
      axes: [
        { stroke: resolveCssVar("--text-muted"), grid: { stroke: resolveCssVar("--gridline") } },
        { stroke: resolveCssVar("--text-muted"), grid: { stroke: resolveCssVar("--gridline") } },
      ],
      series: [
        { label: "t" },
        ...series.map((s, i) => ({
          label: s.label,
          stroke: colors[i],
          width: 2,
          points: { show: false },
        })),
      ],
    };

    const plot = new uPlot(opts, [[], ...series.map(() => [])], containerRef.current);
    plotRef.current = plot;

    const onResize = () => {
      if (containerRef.current) plot.setSize({ width: containerRef.current.clientWidth, height });
    };
    window.addEventListener("resize", onResize);

    return () => {
      window.removeEventListener("resize", onResize);
      plot.destroy();
      plotRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [series.map((s) => s.key).join(","), height, theme]);

  useEffect(() => {
    if (!plotRef.current) return;
    const xs = data.map((d) => d.t);
    const ys = series.map((s) => data.map((d) => d[s.key] as number));
    plotRef.current.setData([xs, ...ys]);
  }, [data, series]);

  return (
    <div>
      <div className="chart-legend">
        {series.map((s) => (
          <span className="chart-legend-item" key={String(s.key)}>
            <span
              className="chart-legend-swatch"
              style={{ background: `var(${s.colorVar})` }}
            />
            {s.label}
          </span>
        ))}
      </div>
      <div className="chart-container" ref={containerRef} />
    </div>
  );
}
