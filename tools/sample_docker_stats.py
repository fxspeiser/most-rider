#!/usr/bin/env python3
"""M7: CPU/memory capture during a benchmark run — the metric every prior
milestone's reports were missing (benchmarks/methodology.md only ever
disclosed latency). Polls `docker stats --no-stream` for the named
containers at a fixed interval for a fixed duration and writes a JSON
summary (min/mean/max CPU% and memory) alongside the latency report.

Not a C++ instrumentation addition — `docker stats` already gives us this
for free from the daemon, no new code in any zone/service binary needed.
"""
import argparse
import json
import statistics
import subprocess
import sys
import time
from pathlib import Path


def sample_once(containers):
    # --no-stream gives one snapshot per call rather than a live stream —
    # simpler to poll on our own interval than to parse a continuous feed.
    result = subprocess.run(
        ["docker", "stats", "--no-stream", "--format",
         "{{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}"] + containers,
        capture_output=True, text=True, timeout=10,
    )
    samples = {}
    for line in result.stdout.strip().splitlines():
        parts = line.split("\t")
        if len(parts) != 3:
            continue
        name, cpu_pct, mem_usage = parts
        samples[name] = {
            "cpu_pct": float(cpu_pct.strip().rstrip("%")),
            # MemUsage looks like "12.34MiB / 1.943GiB" — keep the used side only.
            "mem_used": mem_usage.split("/")[0].strip(),
        }
    return samples


def mem_to_mib(mem_str):
    value = float("".join(c for c in mem_str if c.isdigit() or c == "."))
    if "GiB" in mem_str:
        return value * 1024
    if "KiB" in mem_str:
        return value / 1024
    return value  # assume MiB


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("containers", nargs="+", help="Container names to sample, e.g. most-rider-central")
    parser.add_argument("--duration-s", type=float, default=30)
    parser.add_argument("--interval-s", type=float, default=1.0)
    parser.add_argument("--out-json", type=Path, required=True)
    args = parser.parse_args()

    series = {c: {"cpu_pct": [], "mem_mib": []} for c in args.containers}
    # Real wall-clock time, not an assumed per-iteration increment — a slow
    # `docker stats` call (it shells out to the daemon) previously meant
    # actual elapsed time could run well ahead of a counter that just added
    # interval_s every loop, silently under-sampling (9 samples captured in
    # a 35s window that should have given ~35). Caught by noticing the
    # sample count was a fifth of what the duration/interval implied.
    start = time.monotonic()
    while time.monotonic() - start < args.duration_s:
        loop_start = time.monotonic()
        samples = sample_once(args.containers)
        for name, values in samples.items():
            if name not in series:
                continue
            mem_mib = mem_to_mib(values["mem_used"])
            # A live process never reports exactly 0 memory — a 0 reading
            # is `docker stats` catching the container mid-startup (created
            # but not yet running) or mid-teardown, not a real measurement.
            # Caught by cross-checking this script's aggregate against a
            # manual `docker stats` spot-check that showed ~2MiB while the
            # aggregate claimed a 0.54MiB mean — the zeros were dragging it
            # down.
            if mem_mib <= 0:
                continue
            series[name]["cpu_pct"].append(values["cpu_pct"])
            series[name]["mem_mib"].append(mem_mib)
        remaining = args.interval_s - (time.monotonic() - loop_start)
        if remaining > 0:
            time.sleep(remaining)

    summary = {}
    for name, values in series.items():
        if not values["cpu_pct"]:
            summary[name] = {"error": "no samples captured — container not running during the poll window?"}
            continue
        summary[name] = {
            "cpu_pct": {
                "min": round(min(values["cpu_pct"]), 2),
                "mean": round(statistics.mean(values["cpu_pct"]), 2),
                "max": round(max(values["cpu_pct"]), 2),
            },
            "mem_mib": {
                "min": round(min(values["mem_mib"]), 2),
                "mean": round(statistics.mean(values["mem_mib"]), 2),
                "max": round(max(values["mem_mib"]), 2),
            },
            "samples": len(values["cpu_pct"]),
        }

    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    args.out_json.write_text(json.dumps(summary, indent=2) + "\n")
    print(f"PASS: wrote {args.out_json}")
    for name, s in summary.items():
        if "error" in s:
            print(f"  {name}: {s['error']}", file=sys.stderr)
        else:
            print(f"  {name}: cpu mean={s['cpu_pct']['mean']}% mem mean={s['mem_mib']['mean']}MiB (n={s['samples']})")


if __name__ == "__main__":
    main()
