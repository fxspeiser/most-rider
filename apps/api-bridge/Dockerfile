# Python control plane (ADR-0001, ADR-0007) — never touches DDS directly.
# docker.io + docker-compose-v2 install from Ubuntu's default repos (no
# external Docker apt repo needed) to give this container the `docker` /
# `docker compose` CLI it needs for fault-control endpoints.
FROM ubuntu:24.04

RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    python3 python3-venv docker.io docker-compose-v2 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY apps/api-bridge/requirements.txt .
RUN python3 -m venv /venv && /venv/bin/pip install --no-cache-dir -r requirements.txt

COPY apps/api-bridge/app ./app

EXPOSE 8282
CMD ["/venv/bin/uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8282"]
