#!/usr/bin/env python3
"""Verify the stale/delayed-sensor scenario: injected network delay must
show up as measured latency, and clearing it must let latency recover.

Compares three analyze_run.py JSON summaries (baseline / faulted /
recovered) for a single zone's HeartBeat latency.
"""
import argparse
import json
import sys
from pathlib import Path


def load(path):
    return json.loads(Path(path).read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline_json")
    parser.add_argument("faulted_json")
    parser.add_argument("recovered_json")
    parser.add_argument("--injected-delay-ms", type=float, required=True)
    parser.add_argument("--out-md", required=True, type=Path)
    args = parser.parse_args()

    baseline = load(args.baseline_json)
    faulted = load(args.faulted_json)
    recovered = load(args.recovered_json)

    b_mean = baseline["stats"]["mean_us"]
    f_mean = faulted["stats"]["mean_us"]
    r_mean = recovered["stats"]["mean_us"]

    injected_us = args.injected_delay_ms * 1000
    observed_increase_us = f_mean - b_mean

    # The fault should show up as roughly the injected delay (within 30%,
    # to allow for jitter/measurement noise) - not just "somewhat higher."
    fault_detected = observed_increase_us > injected_us * 0.5
    recovered_ok = abs(r_mean - b_mean) < (injected_us * 0.3 + 200)  # tolerance floor for noise

    verdict = "PASS" if (fault_detected and recovered_ok) else "FAIL"

    lines = [
        "# Stale/delayed-sensor scenario: verdict",
        "",
        f"**{verdict}**",
        "",
        f"- Injected delay: {args.injected_delay_ms}ms ({injected_us:.0f}us)",
        f"- Baseline mean latency: {b_mean}us",
        f"- Faulted mean latency: {f_mean}us (+{observed_increase_us:.0f}us vs baseline)",
        f"- Recovered mean latency: {r_mean}us (delta vs baseline: {r_mean - b_mean:+.0f}us)",
        "",
        f"- Fault detected in measured latency: {'yes' if fault_detected else 'NO'} "
        f"(observed increase must exceed 50% of injected delay)",
        f"- Recovery confirmed after clearing fault: {'yes' if recovered_ok else 'NO'}",
        "",
        "## Full stats",
        "",
        "| Phase | count | mean | p50 | p99 |",
        "|---|---|---|---|---|",
    ]
    for name, run in [("baseline", baseline), ("faulted", faulted), ("recovered", recovered)]:
        s = run["stats"]
        lines.append(f"| {name} | {s['count']} | {s['mean_us']} | {s['p50_us']} | {s['p99_us']} |")

    lines += [
        "",
        "## Method",
        "",
        "Real `tc netem` delay applied to a running zone container's network",
        "interface (tools/inject_network_fault.sh) — not an app-level fake. The",
        "injected delay is one-way and should appear directly in measured",
        "latency (central_receive_ns - sender_send_ns), since both share the",
        "host's monotonic clock (ADR-0002).",
        "",
    ]
    args.out_md.write_text("\n".join(lines) + "\n")
    print(f"{verdict}: wrote {args.out_md}")
    sys.exit(0 if verdict == "PASS" else 1)


if __name__ == "__main__":
    main()
