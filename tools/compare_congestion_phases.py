#!/usr/bin/env python3
"""Compare the three congestion-scenario phases and render a verdict.

Reads the three JSON summaries tools/analyze_run.py produced for the
baseline / no-priority / priority phases and asks one honest question: did
priority QoS measurably help under the *same* flood? This can come back
PASS, FAIL, or INCONCLUSIVE (if the flood never actually stressed the
system) — a forced PASS/FAIL on a result that doesn't support one would be
exactly the kind of unsupported claim crosscheck/README.md's honesty rules
exist to prevent.
"""
import argparse
import json
from pathlib import Path


def load(path):
    return json.loads(Path(path).read_text())


def render(baseline, no_priority, priority):
    b_p99 = baseline["stats"]["p99_us"]
    np_p99 = no_priority["stats"]["p99_us"]
    p_p99 = priority["stats"]["p99_us"]

    degradation_no_priority = np_p99 - b_p99
    degradation_priority = p_p99 - b_p99

    flood_was_stressful = degradation_no_priority > (0.25 * b_p99)  # >25% worse than baseline

    if not flood_was_stressful:
        verdict = "INCONCLUSIVE"
        reasoning = (
            f"The flood barely moved the no-priority phase's p99 ({b_p99}us -> {np_p99}us) — "
            "the system wasn't actually stressed enough to test the claim. Increase "
            "LOAD_RATE_HZ/LOAD_PAYLOAD_BYTES and re-run before trusting this result."
        )
    elif degradation_priority <= 0.5 * degradation_no_priority:
        verdict = "PASS"
        reasoning = (
            f"Under the same flood, priority QoS held p99 degradation to {degradation_priority:.1f}us "
            f"vs {degradation_no_priority:.1f}us without it — priority measurably helped."
        )
    else:
        verdict = "FAIL"
        reasoning = (
            f"Priority QoS did not meaningfully reduce degradation: {degradation_priority:.1f}us "
            f"vs {degradation_no_priority:.1f}us without it. Investigate before claiming priority survival."
        )

    p999_caveat = ""
    b_p999, np_p999, p_p999 = (
        baseline["stats"]["p999_us"], no_priority["stats"]["p999_us"], priority["stats"]["p999_us"]
    )
    b_max, np_max, p_max = (
        baseline["stats"]["max_us"], no_priority["stats"]["max_us"], priority["stats"]["max_us"]
    )
    if verdict == "PASS" and (p_p999 > np_p999 or p_max > np_max):
        p999_caveat = (
            "\n**Caveat, disclosed rather than omitted:** while p99 improved, the far tail did "
            f"not — p99.9 was {p_p999}us with priority QoS vs {np_p999}us without it, and max was "
            f"{p_max}us vs {np_max}us. RELIABLE QoS likely trades occasional ACKNACK-retransmission "
            "stalls for a better typical-tail (p99) outcome. Investigate before claiming priority QoS "
            "improves *every* percentile — it does not, in this run.\n"
        )

    lines = [
        "# Congestion-with-priority-survival: verdict",
        "",
        f"**{verdict}** — {reasoning}",
        p999_caveat,
        "## p99 latency by phase (microseconds)",
        "",
        "| Phase | p99 (us) | vs. baseline |",
        "|---|---|---|",
        f"| baseline (no flood, priority QoS on) | {b_p99} | — |",
        f"| no-priority (flood on, priority QoS off) | {np_p99} | +{degradation_no_priority:.1f} |",
        f"| priority (flood on, priority QoS on) | {p_p99} | +{degradation_priority:.1f} |",
        "",
        "## Full stats per phase",
        "",
        "| Phase | count | p50 | p90 | p95 | p99 | p99.9 | max |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for name, run in [("baseline", baseline), ("no-priority", no_priority), ("priority", priority)]:
        s = run["stats"]
        lines.append(
            f"| {name} | {s['count']} | {s['p50_us']} | {s['p90_us']} | {s['p95_us']} | "
            f"{s['p99_us']} | {s['p999_us']} | {s['max_us']} |"
        )

    lines += [
        "",
        "## Scope",
        "",
        "This tests layers 1-2 of the priority stack (app-level traffic class +",
        "DDS RELIABLE/deadline/KEEP_LAST QoS) — see ADR-0006. It does not test",
        "layer 4 (DSCP + tc queueing), which is deferred. `transport_priority`",
        "(layer 3) is set but not claimed to enforce anything at the network",
        "layer on this single-host Docker bridge — advisory only.",
        "",
    ]
    return "\n".join(lines) + "\n", verdict


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline_json")
    parser.add_argument("no_priority_json")
    parser.add_argument("priority_json")
    parser.add_argument("--out-md", required=True, type=Path)
    args = parser.parse_args()

    baseline = load(args.baseline_json)
    no_priority = load(args.no_priority_json)
    priority = load(args.priority_json)

    report, verdict = render(baseline, no_priority, priority)
    args.out_md.write_text(report)
    print(f"{verdict}: wrote {args.out_md}")


if __name__ == "__main__":
    main()
