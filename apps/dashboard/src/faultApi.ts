// Thin wrappers over api-bridge's fault-control endpoints
// (apps/api-bridge/app/main.py). Same-origin — no base URL needed.

async function call(method: string, path: string, body?: unknown) {
  const res = await fetch(path, {
    method,
    headers: body ? { "Content-Type": "application/json" } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  if (!res.ok) {
    const detail = await res.text();
    throw new Error(`${method} ${path} failed: ${res.status} ${detail}`);
  }
  return res.json();
}

export const faultApi = {
  stopZone: (zone: string) => call("POST", `/api/faults/zones/${zone}/stop`),
  startZone: (zone: string) => call("POST", `/api/faults/zones/${zone}/start`),
  injectDelay: (zone: string, delayMs: number, jitterMs = 20) =>
    call("POST", `/api/faults/network/${zone}`, { delay_ms: delayMs, jitter_ms: jitterMs }),
  clearDelay: (zone: string) => call("DELETE", `/api/faults/network/${zone}`),
  startCongestion: () => call("POST", "/api/faults/congestion"),
  stopCongestion: () => call("DELETE", "/api/faults/congestion"),
  clearDiagnostics: () => call("DELETE", "/api/diagnostics"),
};
