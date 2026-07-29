#!/usr/bin/env bash
# M7: runs central + front-zone as two plain processes inside this one
# container, talking over loopback with DDS's default discovery — no
# unicast-peers config needed, since there's no bridge network or separate
# containers to route around. This isolates "separate containers + bridge
# network" as the only variable vs. the standard docker-compose golden run
# (tools/run_golden_benchmark.sh), holding OS/kernel/Docker-runtime
# constant — unlike tools/run_native_benchmark.sh, which also changes OS.
set -euo pipefail

RECORD_PATH="${RECORD_PATH:-/data/linux-single-container.jsonl}"
RECORD_DURATION_S="${RECORD_DURATION_S:-30}"

RECORD_PATH="$RECORD_PATH" RECORD_DURATION_S="$RECORD_DURATION_S" central &
CENTRAL_PID=$!

sleep 1
ZONE_ID=front-zone PUBLISH_PERIOD_MS=100 zone-runtime &
ZONE_PID=$!

wait "$CENTRAL_PID"
kill "$ZONE_PID" 2>/dev/null || true
