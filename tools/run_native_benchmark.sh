#!/usr/bin/env bash
# M7: native (no-container) benchmark — the "native vs Docker" axis the
# flight-plan panel asked for (benchmarks/methodology.md's honesty rule:
# "any native-vs-container comparison must be run and reported explicitly,
# never inferred"). Builds Cyclone DDS from source (no package manager has
# it prebuilt for macOS — confirmed, not assumed) into a cached local
# prefix, builds central+zone-runtime natively against it, and runs the
# same golden-run measurement as tools/run_golden_benchmark.sh with no
# Docker involved at all.
#
# Caveat this script does NOT resolve: run on macOS, this compares
# "no containers" against the Docker reports' "Linux in Docker Desktop's
# VM" — two variables change at once (containerization AND OS), not one.
# See tools/run_linux_process_benchmark.sh for the single-variable version
# (same Linux environment, process vs. separate containers).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

CACHE_DIR="${NATIVE_BENCH_CACHE:-.native-bench-cache}"
CDDS_TAG="0.10.4"
CDDS_SRC="$CACHE_DIR/cyclonedds-src"
CDDS_PREFIX="$CACHE_DIR/cyclonedds-install"
BUILD_DIR="$CACHE_DIR/build"
DURATION_S="${NATIVE_BENCH_DURATION_S:-30}"
RUN_ID="${1:-native-macos-arm64}"

mkdir -p "$CACHE_DIR" benchmarks/runs benchmarks/reports

if [ ! -f "$CDDS_PREFIX/lib/cmake/CycloneDDS/CycloneDDSConfig.cmake" ]; then
  echo "Cyclone DDS not cached — building from source (one-time cost, ~5s)..."
  rm -rf "$CDDS_SRC"
  git clone --depth 1 --branch "$CDDS_TAG" \
    https://github.com/eclipse-cyclonedds/cyclonedds.git "$CDDS_SRC"
  # ENABLE_SECURITY=NO: this project doesn't use DDS-Security anywhere (a
  # disclosed limitation, see docs/architecture/security-limitations.md).
  # ENABLE_SSL=NO: a *separate* flag gating TLS-over-TCP transport — left
  # at its CMake default, it links OpenSSL::SSL whenever OPENSSL_FOUND is
  # true, which fails on this machine (Homebrew's openssl is keg-only, not
  # on the default linker path) — a real link failure hit on the first two
  # attempts here, only the second flag actually fixed it.
  cmake -S "$CDDS_SRC" -B "$CDDS_SRC/build" \
    -DCMAKE_INSTALL_PREFIX="$(pwd)/$CDDS_PREFIX" -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
    -DENABLE_SECURITY=NO -DENABLE_SSL=NO
  cmake --build "$CDDS_SRC/build" --target install --parallel
else
  echo "Using cached Cyclone DDS build at $CDDS_PREFIX"
fi

echo "Building central + zone-runtime natively..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(pwd)/$CDDS_PREFIX" >/dev/null
cmake --build "$BUILD_DIR" --target central zone-runtime --parallel >/dev/null

export DYLD_LIBRARY_PATH="$(pwd)/$CDDS_PREFIX/lib:${DYLD_LIBRARY_PATH:-}"
export LD_LIBRARY_PATH="$(pwd)/$CDDS_PREFIX/lib:${LD_LIBRARY_PATH:-}"

RUN_FILE="benchmarks/runs/${RUN_ID}.jsonl"
rm -f "$RUN_FILE"

cleanup() {
  [ -n "${FRONTZONE_PID:-}" ] && kill "$FRONTZONE_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "Running native central + front-zone for ${DURATION_S}s..."
RECORD_PATH="$RUN_FILE" RECORD_DURATION_S="$DURATION_S" "$BUILD_DIR/central" &
CENTRAL_PID=$!
sleep 1
ZONE_ID=front-zone PUBLISH_PERIOD_MS=100 "$BUILD_DIR/zone-runtime" &
FRONTZONE_PID=$!

wait "$CENTRAL_PID"

python3 tools/analyze_run.py "$RUN_FILE" \
  --run-id "$RUN_ID" \
  --out-md "benchmarks/reports/${RUN_ID}.md" \
  --out-json "benchmarks/reports/${RUN_ID}.json" \
  --context native \
  --cyclonedds-version "${CDDS_TAG} (built from source, github.com/eclipse-cyclonedds/cyclonedds, not the Ubuntu apt package used in Docker runs)"
