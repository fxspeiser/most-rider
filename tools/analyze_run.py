#!/usr/bin/env python3
"""Turn a raw JSONL sample capture from central into a golden-run report.

Reads {"zone", "seq", "latency_us"} lines (see
middleware/metrics/sample_recorder.hpp), computes exact percentiles from the
raw samples (see ADR-0003 for why raw capture instead of HDR bucketing at
this scale), and writes a markdown report plus a JSON summary sidecar for
later dashboard consumption (M6).

Every claim in the report is either a computed statistic or an environment
fact gathered here — nothing is hand-entered, per crosscheck/README.md's
honesty rules.
"""
import argparse
import json
import platform
import statistics
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def percentile(sorted_values, pct):
    if not sorted_values:
        return None
    k = (len(sorted_values) - 1) * (pct / 100)
    f = int(k)
    c = min(f + 1, len(sorted_values) - 1)
    if f == c:
        return sorted_values[f]
    return sorted_values[f] + (sorted_values[c] - sorted_values[f]) * (k - f)


def load_samples(path):
    samples = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            samples.append(json.loads(line))
    return samples


def tool_version(cmd):
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
        return out.stdout.strip() or out.stderr.strip()
    except Exception as exc:  # noqa: BLE001 - environment disclosure is best-effort
        return f"(unavailable: {exc})"


def docker_info_field(go_template):
    return tool_version(["docker", "info", "--format", go_template])


def gather_environment(context, cyclonedds_version):
    """context: 'docker-compose' (default) or 'native'. The two run
    contexts have genuinely different authoritative environments — a
    native run has no docker daemon to query, and hardcoding the Docker
    image's cyclonedds version into a native report would be a real,
    misleading inaccuracy (caught by actually running a native benchmark
    and reading the resulting report — not by inspection)."""
    base = {
        "captured_at_utc": datetime.now(timezone.utc).isoformat(),
        "run_context": context,
        "python_version": sys.version.split()[0],
        "cyclonedds_version": cyclonedds_version,
        "measurement_note": (
            "Single host; latency is (central's CLOCK_MONOTONIC at receipt) "
            "minus (publisher's CLOCK_MONOTONIC at send). Valid because both "
            "processes share the kernel's monotonic clock (ADR-0002) - NOT valid "
            "across two physical hosts without PTP or an RTT methodology."
        ),
    }

    if context == "native":
        # The orchestrating host IS the run environment for a native run —
        # no container layer to distinguish it from.
        base["host_platform"] = platform.platform()
        base["host_machine"] = platform.machine()
        return base

    # docker-compose context: the docker daemon is the authoritative
    # environment, not necessarily this script's host (e.g. Python under
    # Rosetta on Apple Silicon can report x86_64 here while containers run
    # native arm64 — see docker_daemon_* below for the environment that
    # actually ran the timed containers).
    base["orchestrating_host_platform"] = platform.platform()
    base["orchestrating_host_machine"] = platform.machine()
    base["docker_version"] = tool_version(["docker", "--version"])
    base["docker_compose_version"] = tool_version(["docker", "compose", "version"])
    base["docker_daemon_architecture"] = docker_info_field("{{.Architecture}}")
    base["docker_daemon_os"] = docker_info_field("{{.OSType}} / {{.OperatingSystem}}")
    base["docker_daemon_cpus"] = docker_info_field("{{.NCPU}}")
    return base


def compute_stats(samples):
    latencies = sorted(s["latency_us"] for s in samples)
    if not latencies:
        return None
    return {
        "count": len(latencies),
        "min_us": latencies[0],
        "max_us": latencies[-1],
        "mean_us": round(statistics.mean(latencies), 1),
        "stddev_us": round(statistics.pstdev(latencies), 1) if len(latencies) > 1 else 0.0,
        "p50_us": round(percentile(latencies, 50), 1),
        "p90_us": round(percentile(latencies, 90), 1),
        "p95_us": round(percentile(latencies, 95), 1),
        "p99_us": round(percentile(latencies, 99), 1),
        "p999_us": round(percentile(latencies, 99.9), 1),
    }


def render_markdown(run_id, stats, env, per_zone_counts, source_path):
    lines = [
        f"# Golden run: {run_id}",
        "",
        f"Raw samples: `{source_path}` ({stats['count']} total). "
        "Regenerate with `tools/run_golden_benchmark.sh`.",
        "",
        "## Latency (microseconds, one-way, front-zone -> central)",
        "",
        "| Stat | Value (us) |",
        "|---|---|",
        f"| count | {stats['count']} |",
        f"| min | {stats['min_us']} |",
        f"| mean | {stats['mean_us']} |",
        f"| stddev | {stats['stddev_us']} |",
        f"| p50 | {stats['p50_us']} |",
        f"| p90 | {stats['p90_us']} |",
        f"| p95 | {stats['p95_us']} |",
        f"| p99 | {stats['p99_us']} |",
        f"| p99.9 | {stats['p999_us']} |",
        f"| max | {stats['max_us']} |",
        "",
        "## Samples per zone",
        "",
        "| Zone | Samples |",
        "|---|---|",
    ]
    for zone, count in sorted(per_zone_counts.items()):
        lines.append(f"| {zone} | {count} |")

    lines += [
        "",
        "## Environment (captured at run time, not hand-entered)",
        "",
        "| Field | Value |",
        "|---|---|",
    ]
    for key, value in env.items():
        lines.append(f"| {key} | {value} |")

    lines += [
        "",
        "## Methodology and scope",
        "",
        "No latency target is pre-committed for this run (ADR-0001/plan "
        "synthesis: targets are ADR design goals, not public claims until "
        "measured). This is a **baseline-derived envelope** — a starting "
        "point for M4's priority-under-load comparison and M7's honest "
        "benchmark matrix, not an industry comparison. See "
        "`benchmarks/methodology.md` for the full disclosure rules.",
        "",
    ]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Path to the raw JSONL sample capture")
    parser.add_argument("--run-id", required=True, help="Identifier for this run, e.g. golden-run-1")
    parser.add_argument("--out-md", type=Path, required=True, help="Output markdown report path")
    parser.add_argument("--out-json", type=Path, required=True, help="Output JSON summary path")
    parser.add_argument(
        "--context", choices=["docker-compose", "docker-single-container", "native"],
        default="docker-compose",
        help=(
            "Run context — changes which environment facts are authoritative "
            "(default: docker-compose). 'docker-single-container' still discloses "
            "docker daemon facts (it's still Docker) but is labeled distinctly from "
            "'docker-compose' (separate containers on the bridge network) — see "
            "tools/run_linux_process_benchmark.sh."
        ),
    )
    parser.add_argument(
        "--cyclonedds-version", default="0.10.4 (Ubuntu 24.04 apt package, see deploy/Dockerfile)",
        help="Cyclone DDS version/provenance string for this run's environment — override for non-Docker contexts",
    )
    args = parser.parse_args()

    samples = load_samples(args.input)
    if not samples:
        print(f"FAIL: no samples found in {args.input}", file=sys.stderr)
        sys.exit(1)

    stats = compute_stats(samples)
    per_zone_counts = {}
    for s in samples:
        per_zone_counts[s["zone"]] = per_zone_counts.get(s["zone"], 0) + 1
    env = gather_environment(args.context, args.cyclonedds_version)

    args.out_md.parent.mkdir(parents=True, exist_ok=True)
    args.out_md.write_text(render_markdown(args.run_id, stats, env, per_zone_counts, args.input))

    summary = {
        "run_id": args.run_id,
        "source": str(args.input),
        "stats": stats,
        "per_zone_counts": per_zone_counts,
        "environment": env,
    }
    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    args.out_json.write_text(json.dumps(summary, indent=2) + "\n")

    print(f"PASS: wrote {args.out_md} and {args.out_json}")
    print(f"  p50={stats['p50_us']}us p99={stats['p99_us']}us p99.9={stats['p999_us']}us "
          f"(n={stats['count']})")


if __name__ == "__main__":
    main()
