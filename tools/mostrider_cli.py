#!/usr/bin/env python3
"""CLI for the most-rider API (M5) — stdlib-only, no dependencies beyond
Python itself, so it runs on the host without a venv. Talks to api-bridge's
REST surface; see apps/api-bridge/app/main.py and scenarios/README.md for
what each fault-control command actually does.

Examples:
    tools/mostrider_cli.py zones
    tools/mostrider_cli.py diagnostics
    tools/mostrider_cli.py diagnostics --clear
    tools/mostrider_cli.py fault stop-zone rear-zone
    tools/mostrider_cli.py fault inject-delay rear-zone 300
    tools/mostrider_cli.py fault clear-delay rear-zone
    tools/mostrider_cli.py fault congestion start
"""
import argparse
import json
import os
import sys
import urllib.error
import urllib.request

DEFAULT_BASE_URL = os.environ.get("MOSTRIDER_API_URL", "http://localhost:8282")


def request(base_url, method, path, body=None):
    url = base_url.rstrip("/") + path
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    if data is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode(errors="replace")
        print(f"error: HTTP {exc.code}: {detail}", file=sys.stderr)
        sys.exit(1)
    except urllib.error.URLError as exc:
        print(f"error: could not reach {url}: {exc.reason}", file=sys.stderr)
        sys.exit(1)


def show(data):
    print(json.dumps(data, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL, help=f"api-bridge base URL (default: {DEFAULT_BASE_URL})")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("health")
    sub.add_parser("zones")
    sub.add_parser("propulsion")
    sub.add_parser("energy")
    sub.add_parser("body")

    diag = sub.add_parser("diagnostics")
    diag.add_argument("--clear", action="store_true", help="clear diagnostics (UDS-inspired ClearDTC)")

    fault = sub.add_parser("fault")
    fault_sub = fault.add_subparsers(dest="fault_command", required=True)

    stop_zone = fault_sub.add_parser("stop-zone")
    stop_zone.add_argument("zone")

    start_zone = fault_sub.add_parser("start-zone")
    start_zone.add_argument("zone")

    inject_delay = fault_sub.add_parser("inject-delay")
    inject_delay.add_argument("zone")
    inject_delay.add_argument("delay_ms", type=int)
    inject_delay.add_argument("--jitter-ms", type=int, default=20)

    clear_delay = fault_sub.add_parser("clear-delay")
    clear_delay.add_argument("zone")

    congestion = fault_sub.add_parser("congestion")
    congestion.add_argument("action", choices=["start", "stop"])

    args = parser.parse_args()

    if args.command == "health":
        show(request(args.base_url, "GET", "/api/health"))
    elif args.command == "zones":
        show(request(args.base_url, "GET", "/api/zones"))
    elif args.command == "propulsion":
        show(request(args.base_url, "GET", "/api/propulsion"))
    elif args.command == "energy":
        show(request(args.base_url, "GET", "/api/energy"))
    elif args.command == "body":
        show(request(args.base_url, "GET", "/api/body"))
    elif args.command == "diagnostics":
        if args.clear:
            show(request(args.base_url, "DELETE", "/api/diagnostics"))
        else:
            show(request(args.base_url, "GET", "/api/diagnostics"))
    elif args.command == "fault":
        if args.fault_command == "stop-zone":
            show(request(args.base_url, "POST", f"/api/faults/zones/{args.zone}/stop"))
        elif args.fault_command == "start-zone":
            show(request(args.base_url, "POST", f"/api/faults/zones/{args.zone}/start"))
        elif args.fault_command == "inject-delay":
            show(request(
                args.base_url, "POST", f"/api/faults/network/{args.zone}",
                {"delay_ms": args.delay_ms, "jitter_ms": args.jitter_ms},
            ))
        elif args.fault_command == "clear-delay":
            show(request(args.base_url, "DELETE", f"/api/faults/network/{args.zone}"))
        elif args.fault_command == "congestion":
            method = "POST" if args.action == "start" else "DELETE"
            show(request(args.base_url, method, "/api/faults/congestion"))


if __name__ == "__main__":
    main()
