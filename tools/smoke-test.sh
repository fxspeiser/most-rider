#!/usr/bin/env bash
# M0 exit criterion: front-zone and central discover each other and exchange
# HeartBeat messages across the Docker bridge network (ADR-0002), using the
# committed Cyclone DDS unicast-peer config — no manual steps, no multicast.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

TIMEOUT_S="${SMOKE_TEST_TIMEOUT_S:-60}"

cleanup() {
  echo "--- central logs ---"
  docker compose logs central || true
  echo "--- front-zone logs ---"
  docker compose logs front-zone || true
  docker compose down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "Building and starting the M0 stack..."
docker compose up --build -d

echo "Waiting up to ${TIMEOUT_S}s for central to log a received heartbeat..."
elapsed=0
until docker compose logs central 2>/dev/null | grep -q "received heartbeat"; do
  if [ "$elapsed" -ge "$TIMEOUT_S" ]; then
    echo "FAIL: no heartbeat received within ${TIMEOUT_S}s"
    exit 1
  fi
  sleep 2
  elapsed=$((elapsed + 2))
done

echo "PASS: central received at least one heartbeat from front-zone"
