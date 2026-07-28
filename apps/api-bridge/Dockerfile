# Python control plane (ADR-0001, ADR-0007) — never touches DDS directly.
# docker.io + docker-compose-v2 install from Ubuntu's default repos (no
# external Docker apt repo needed) to give this container the `docker` /
# `docker compose` CLI it needs for fault-control endpoints.
#
# M6 (ADR-0008): the dashboard (apps/dashboard, React+Vite+TS) is built in
# its own stage and copied in as static files — served from the same
# origin as the REST/WebSocket API so there's no CORS configuration and no
# second port to expose.
FROM node:20-slim AS dashboard-builder

# Same relative layout as the host repo (design/ next to apps/dashboard/)
# so the identical predev/prebuild `cp ../../design/tokens.css` script
# works unmodified in both Docker and local dev — see ADR-0008.
WORKDIR /repo
COPY design ./design
COPY apps/dashboard ./apps/dashboard
WORKDIR /repo/apps/dashboard
RUN npm ci
RUN npm run build

FROM ubuntu:24.04

RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    python3 python3-venv docker.io docker-compose-v2 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY apps/api-bridge/requirements.txt .
RUN python3 -m venv /venv && /venv/bin/pip install --no-cache-dir -r requirements.txt

COPY apps/api-bridge/app ./app
COPY --from=dashboard-builder /repo/apps/dashboard/dist ./static

EXPOSE 8282
CMD ["/venv/bin/uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8282"]
