#!/usr/bin/env bash
# Golden run #1 (M1 exit criterion): a scripted, repeatable benchmark run
# that records raw front-zone -> central latency samples and produces a
# committed report. See benchmarks/methodology.md for the honesty rules
# this report follows.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

RUN_ID="${1:-golden-run-1}"
DURATION_S="${GOLDEN_RUN_DURATION_S:-30}"
RUN_FILE="${RUN_ID}.jsonl"

export GOLDEN_RUN_FILE="$RUN_FILE"
export GOLDEN_RUN_DURATION_S="$DURATION_S"

mkdir -p benchmarks/runs benchmarks/reports

cleanup() {
  docker compose -f docker-compose.yml -f docker-compose.benchmark.yml down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "Running '${RUN_ID}' for ${DURATION_S}s..."
docker compose -f docker-compose.yml -f docker-compose.benchmark.yml up --build \
  --abort-on-container-exit --exit-code-from central

python3 tools/analyze_run.py "benchmarks/runs/${RUN_FILE}" \
  --run-id "$RUN_ID" \
  --out-md "benchmarks/reports/${RUN_ID}.md" \
  --out-json "benchmarks/reports/${RUN_ID}.json"

echo "Report: benchmarks/reports/${RUN_ID}.md"
