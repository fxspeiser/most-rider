#!/usr/bin/env bash
# M4 centerpiece: congestion-with-priority-survival (ADR-0006). Runs
# propulsion-monitor for three phases and compares p99 latency across them:
#
#   1. baseline   — no flood, priority QoS on.  (what "normal" looks like)
#   2. no-priority — flood on, priority QoS OFF. (the naive baseline: same
#                    best-effort class as the flood — expected to degrade)
#   3. priority   — flood on, priority QoS ON.   (the actual claim: RELIABLE
#                    + deadline + KEEP_LAST(1) keeps latency close to phase 1
#                    despite the same flood that degraded phase 2)
#
# This is an A/B/A' comparison, not a single number — the claim is the
# *difference* between phases 2 and 3 under an identical flood, not an
# absolute latency figure. See benchmarks/methodology.md.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

PHASE_DURATION_S="${CONGESTION_PHASE_DURATION_S:-30}"
# 20ms (50Hz), not the demo's normal 100ms: 30s at 10Hz only yields ~300
# samples, too few for a stable p99 estimate (a first attempt at this
# scenario came back INCONCLUSIVE partly for this reason — see
# crosscheck/ledger.jsonl). This only affects the scenario run, not the
# normal `docker compose up` demo cadence.
PUBLISH_PERIOD_MS="${CONGESTION_PUBLISH_PERIOD_MS:-20}"
LOAD_RATE_HZ="${LOAD_RATE_HZ:-3000}"
LOAD_PAYLOAD_BYTES="${LOAD_PAYLOAD_BYTES:-4096}"
mkdir -p benchmarks/runs benchmarks/reports

cleanup() {
  # Plain `docker compose down` does NOT stop profile-gated services
  # (load-generator, propulsion-monitor) even if they're running — a real
  # bug caught by actually running this script and checking `docker compose
  # ps` afterward, not by reading the compose file. --profile must be
  # passed to down too, not just up.
  docker compose --profile congestion down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT

run_phase() {
  local phase_name="$1" enable_priority="$2" with_flood="$3"
  local run_file="congestion-${phase_name}.jsonl"

  echo "=== Phase: ${phase_name} (priority_qos=${enable_priority}, flood=${with_flood}) ==="

  cleanup

  ENABLE_PRIORITY_QOS="$enable_priority" \
  CONGESTION_RUN_FILE="$run_file" \
  CONGESTION_RUN_DURATION_S="$PHASE_DURATION_S" \
  CONGESTION_PUBLISH_PERIOD_MS="$PUBLISH_PERIOD_MS" \
    docker compose up --build -d energy-service propulsion-monitor

  if [ "$with_flood" = "true" ]; then
    ENABLE_PRIORITY_QOS="$enable_priority" \
    LOAD_RATE_HZ="$LOAD_RATE_HZ" \
    LOAD_PAYLOAD_BYTES="$LOAD_PAYLOAD_BYTES" \
      docker compose --profile congestion up --build -d load-generator
  fi

  echo "Recording for ${PHASE_DURATION_S}s..."
  sleep "$((PHASE_DURATION_S + 5))"

  python3 tools/analyze_run.py "benchmarks/runs/${run_file}" \
    --run-id "congestion-${phase_name}" \
    --out-md "benchmarks/reports/congestion-${phase_name}.md" \
    --out-json "benchmarks/reports/congestion-${phase_name}.json"
}

run_phase "baseline"     "true"  "false"
run_phase "no-priority"  "false" "true"
run_phase "priority"     "true"  "true"

python3 tools/compare_congestion_phases.py \
  benchmarks/reports/congestion-baseline.json \
  benchmarks/reports/congestion-no-priority.json \
  benchmarks/reports/congestion-priority.json \
  --out-md benchmarks/reports/congestion-survival-summary.md
