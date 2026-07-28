#!/usr/bin/env bash
# M3 exit criterion: energy-service and body-service produce a repeatable,
# correct drive cycle. Runs both for slightly more than one full 90s
# drive-cycle period, then checks the captured output against an
# independent Python re-derivation of the same formula
# (tools/verify_m3_services.py) rather than eyeballing the numbers.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

RUN_S="${M3_VERIFY_DURATION_S:-95}"
WORKDIR="$(mktemp -d)"

cleanup() {
  docker compose logs energy-service --no-color > "$WORKDIR/energy-service.log" 2>/dev/null || true
  docker compose logs body-service --no-color > "$WORKDIR/body-service.log" 2>/dev/null || true
  docker compose stop energy-service body-service >/dev/null 2>&1 || true
  docker compose rm -f energy-service body-service >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "Running energy-service and body-service for ${RUN_S}s (one full drive cycle + margin)..."
docker compose up --build -d energy-service body-service
sleep "$RUN_S"

docker compose logs energy-service --no-color > "$WORKDIR/energy-service.log"
docker compose logs body-service --no-color > "$WORKDIR/body-service.log"

python3 tools/verify_m3_services.py "$WORKDIR/energy-service.log" "$WORKDIR/body-service.log"
