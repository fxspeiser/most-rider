#!/usr/bin/env bash
# M7: the single-variable native-vs-container comparison — same Linux
# environment (same base image, same Docker daemon/VM) as the standard
# docker-compose golden run, but central + front-zone run as two plain
# processes in ONE container instead of two separate containers on the
# bridge network. Isolates "separate containers + bridge network overhead"
# specifically, unlike tools/run_native_benchmark.sh (which also changes
# the OS, a confound disclosed there).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

RUN_ID="${1:-linux-single-container}"
DURATION_S="${LINUX_BENCH_DURATION_S:-30}"
RUN_FILE="${RUN_ID}.jsonl"

mkdir -p benchmarks/runs benchmarks/reports

echo "Building the single-container two-process benchmark image..."
docker build -f deploy/Dockerfile.linux-process-bench -t most-rider-linux-process-bench . >/dev/null

echo "Running for ${DURATION_S}s..."
docker run --rm \
  -v "$(pwd)/benchmarks/runs:/data" \
  -e RECORD_PATH="/data/${RUN_FILE}" \
  -e RECORD_DURATION_S="$DURATION_S" \
  most-rider-linux-process-bench

python3 tools/analyze_run.py "benchmarks/runs/${RUN_FILE}" \
  --run-id "$RUN_ID" \
  --out-md "benchmarks/reports/${RUN_ID}.md" \
  --out-json "benchmarks/reports/${RUN_ID}.json" \
  --context docker-single-container \
  --cyclonedds-version "0.10.4 (Ubuntu 24.04 apt package — same image as the docker-compose runs, but central+front-zone run as two processes in ONE container, not two containers on the bridge network)"
