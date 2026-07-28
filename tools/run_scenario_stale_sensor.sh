#!/usr/bin/env bash
# M4 fault scenario 2: stale/delayed sensor (ADR-0006). Three windows —
# baseline, faulted (real tc netem delay on rear-zone), recovered (fault
# cleared) — each recorded via central's existing golden-run recorder
# (M1) and compared by tools/verify_stale_sensor_scenario.py. rear-zone
# keeps running the whole time: this is a degraded sensor, not a dead one
# (that's the M2 kill/restart scenario).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

WINDOW_S="${STALE_SENSOR_WINDOW_S:-15}"
DELAY_MS="${STALE_SENSOR_DELAY_MS:-300}"
mkdir -p benchmarks/runs benchmarks/reports

cleanup() {
  docker compose down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT
cleanup

echo "Starting rear-zone (kept running for the whole scenario) and central..."
docker compose up --build -d rear-zone
sleep 3 # let discovery settle before the first recording window

record_window() {
  local phase_name="$1"
  local run_file="stale-sensor-${phase_name}.jsonl"

  RECORD_PATH="/data/${run_file}" RECORD_DURATION_S="$WINDOW_S" \
    docker compose up -d --force-recreate central
  sleep "$((WINDOW_S + 5))"

  python3 tools/analyze_run.py "benchmarks/runs/${run_file}" \
    --run-id "stale-sensor-${phase_name}" \
    --out-md "benchmarks/reports/stale-sensor-${phase_name}.md" \
    --out-json "benchmarks/reports/stale-sensor-${phase_name}.json"
}

echo "=== Window: baseline ==="
record_window "baseline"

echo "=== Window: faulted ==="
./tools/inject_network_fault.sh rear-zone "$DELAY_MS" 20
record_window "faulted"

echo "=== Window: recovered ==="
./tools/clear_network_fault.sh rear-zone
sleep 2
record_window "recovered"

python3 tools/verify_stale_sensor_scenario.py \
  benchmarks/reports/stale-sensor-baseline.json \
  benchmarks/reports/stale-sensor-faulted.json \
  benchmarks/reports/stale-sensor-recovered.json \
  --injected-delay-ms "$DELAY_MS" \
  --out-md benchmarks/reports/stale-sensor-summary.md
