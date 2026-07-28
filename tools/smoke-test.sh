#!/usr/bin/env bash
# M2 exit criterion: all four zones (front/rear/cabin/central) discover
# each other across the Docker bridge network (ADR-0002) using the
# committed Cyclone DDS unicast-peer config, and central's discovery module
# marks every peripheral zone "recovered" (ZoneRegistry — ADR-0004). Also
# exercises the stale/recovery transition path that M4's fault scenarios
# depend on, by killing and restarting one zone mid-run.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

TIMEOUT_S="${SMOKE_TEST_TIMEOUT_S:-60}"
ZONES=(front-zone rear-zone cabin-zone)

cleanup() {
  echo "--- central logs (tail) ---"
  docker compose logs central --tail 50 || true
  docker compose down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "Building and starting the M2 stack (4 zones)..."
docker compose up --build -d

echo "Waiting up to ${TIMEOUT_S}s for central to mark every zone recovered..."
elapsed=0
while true; do
  logs="$(docker compose logs central 2>/dev/null)"
  missing=()
  for zone in "${ZONES[@]}"; do
    echo "$logs" | grep -q "\[info\] ${zone} recovered" || missing+=("$zone")
  done
  if [ "${#missing[@]}" -eq 0 ]; then
    break
  fi
  if [ "$elapsed" -ge "$TIMEOUT_S" ]; then
    echo "FAIL: zones never recovered: ${missing[*]}"
    exit 1
  fi
  sleep 2
  elapsed=$((elapsed + 2))
done
echo "PASS: central marked all zones recovered: ${ZONES[*]}"

wait_for_log() {
  local description="$1" pattern="$2" timeout_s="$3"
  local waited=0
  until docker compose logs central 2>/dev/null | grep -q "$pattern"; do
    if [ "$waited" -ge "$timeout_s" ]; then
      echo "FAIL: $description"
      return 1
    fi
    sleep 1
    waited=$((waited + 1))
  done
}

echo "Killing rear-zone to verify stale detection..."
docker compose stop rear-zone >/dev/null
wait_for_log "central never marked rear-zone stale after it stopped" \
  "\[warning\] rear-zone went stale" 15
echo "PASS: central detected rear-zone going stale"

echo "Restarting rear-zone to verify recovery..."
docker compose start rear-zone >/dev/null
waited=0
while [ "$(docker compose logs central 2>/dev/null | grep -c "\[info\] rear-zone recovered")" -lt 2 ]; do
  if [ "$waited" -ge 15 ]; then
    echo "FAIL: central never marked rear-zone recovered after restart"
    exit 1
  fi
  sleep 1
  waited=$((waited + 1))
done
echo "PASS: central detected rear-zone recovery"
