#!/usr/bin/env bash
# M4 fault scenario 2: stale/delayed sensor (ADR-0006). Applies real `tc
# netem` network impairment to a running zone container's interface — not
# an app-level fake delay — so the fault exercises the actual network path.
# Requires the target service to have NET_ADMIN (docker-compose.yml grants
# this to all zone-runtime instances).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

SERVICE="${1:?usage: inject_network_fault.sh <service> <delay_ms> [jitter_ms]}"
DELAY_MS="${2:?usage: inject_network_fault.sh <service> <delay_ms> [jitter_ms]}"
JITTER_MS="${3:-20}"

echo "Injecting ${DELAY_MS}ms +/- ${JITTER_MS}ms delay on ${SERVICE}'s eth0..."
docker compose exec -T "$SERVICE" tc qdisc add dev eth0 root netem delay "${DELAY_MS}ms" "${JITTER_MS}ms"
echo "Injected. Clear with: tools/clear_network_fault.sh ${SERVICE}"
