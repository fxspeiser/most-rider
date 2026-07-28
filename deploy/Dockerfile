# Shared build for all zone-runtime executables (front-zone, central, ...).
# One image definition, one CycloneDDS version, per ADR-0001's "manifest
# configured, not bespoke code" decision — the TARGET build arg selects
# which executable ships in the final layer.
FROM ubuntu:24.04 AS builder

RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    cyclonedds-dev cyclonedds-tools cmake build-essential ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY interfaces/idl ./interfaces/idl
COPY middleware ./middleware
COPY apps ./apps

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

FROM ubuntu:24.04 AS runtime

RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
    libddsc0t64 \
    && rm -rf /var/lib/apt/lists/*

ARG TARGET
ENV TARGET=${TARGET}
COPY --from=builder /src/build/${TARGET} /usr/local/bin/zone-runtime

ENTRYPOINT ["/usr/local/bin/zone-runtime"]
