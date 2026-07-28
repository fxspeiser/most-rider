#!/usr/bin/env bash
# Clears whatever tools/inject_network_fault.sh applied to a service.
set -euo pipefail

SERVICE="${1:?usage: clear_network_fault.sh <service>}"

docker compose exec -T "$SERVICE" tc qdisc del dev eth0 root
echo "Cleared network fault on ${SERVICE}."
