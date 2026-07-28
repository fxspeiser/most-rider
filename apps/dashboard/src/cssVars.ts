// uPlot draws to a <canvas> and needs resolved color strings — canvas
// strokeStyle doesn't resolve CSS custom properties the way DOM styles do.
// design/tokens.css scopes its variables under .viz-root (not :root), so
// resolve against that element specifically.
export function resolveCssVar(name: string): string {
  const root = document.querySelector(".viz-root") ?? document.documentElement;
  const value = getComputedStyle(root).getPropertyValue(name).trim();
  return value || "#888888";
}
