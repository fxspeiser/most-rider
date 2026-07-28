#!/usr/bin/env python3
"""Verify energy-service and body-service against an independent
re-derivation of their documented formulas (see ADR-0005 and the header
comments in apps/energy-service/src/main.cpp and apps/body-service/src/main.cpp).

This is a correctness check, not a sanity eyeball: every logged sample's
speed/torque is compared to what the formula predicts for that exact
timestamp, in a second-language reimplementation. A drift beyond tolerance
means the C++ implementation and its own documented model have diverged.
"""
import argparse
import math
import re
import sys

CYCLE_SPEED_KMH = 60.0
CYCLE_PERIOD_S = 90.0
WHEEL_RADIUS_M = 0.33
INERTIA_TORQUE_SCALE = 900.0

ENERGY_LINE = re.compile(
    r"t=(?P<t>[\d.]+)s speed=(?P<speed>[-\d.]+)kmh torque=(?P<torque>[-\d.]+)Nm "
    r"power=(?P<power>[-\d.]+)kW soc=(?P<soc>[\d.]+)% range=(?P<range>[\d.]+)km"
)
BODY_LINE = re.compile(
    r"t=(?P<t>[\d.]+)s door_open=(?P<door>true|false) headlights_on=(?P<lights>true|false)"
)


def expected_speed_kmh(t_s):
    omega = 2.0 * math.pi / CYCLE_PERIOD_S
    return CYCLE_SPEED_KMH * 0.5 * (1.0 - math.cos(omega * t_s))


def expected_torque_nm(t_s):
    omega = 2.0 * math.pi / CYCLE_PERIOD_S
    accel_mss = CYCLE_SPEED_KMH * 0.5 * omega * math.sin(omega * t_s) / 3.6
    return accel_mss * INERTIA_TORQUE_SCALE


def verify_energy(log_text, speed_tol_kmh=0.5, torque_tol_nm=5.0):
    lines = ENERGY_LINE.findall(log_text)
    if not lines:
        return False, "no energy-service samples found in log"

    checked = 0
    for t, speed, torque, _power, soc, _range_km in lines:
        t_s = float(t)
        speed = float(speed)
        torque = float(torque)
        soc = float(soc)

        exp_speed = expected_speed_kmh(t_s)
        exp_torque = expected_torque_nm(t_s)

        if abs(speed - exp_speed) > speed_tol_kmh:
            return False, f"speed drift at t={t_s}s: got {speed}, expected {exp_speed:.2f}"
        if abs(torque - exp_torque) > torque_tol_nm:
            return False, f"torque drift at t={t_s}s: got {torque}, expected {exp_torque:.2f}"
        if not (0.0 <= speed <= CYCLE_SPEED_KMH + 1.0):
            return False, f"speed out of bounds at t={t_s}s: {speed}"
        if not (0.0 <= soc <= 100.0):
            return False, f"SoC out of bounds at t={t_s}s: {soc}"
        checked += 1

    return True, f"{checked} energy-service samples matched the independent re-derivation"


def verify_body(log_text, expected_min_cycles=2):
    lines = BODY_LINE.findall(log_text)
    if not lines:
        return False, "no body-service samples found in log"

    transitions = 0
    prev_open = None
    for _t, door, lights in lines:
        door_open = door == "true"
        lights_on = lights == "true"
        if door_open != lights_on:
            return False, "headlights_on did not follow door_open (the demo beat is broken)"
        if prev_open is not None and door_open != prev_open:
            transitions += 1
        prev_open = door_open

    cycles_observed = transitions // 2
    if cycles_observed < expected_min_cycles:
        return False, (
            f"only {cycles_observed} door open/close cycles observed, "
            f"expected at least {expected_min_cycles} - run longer"
        )

    return True, f"{len(lines)} body-service samples, {cycles_observed} door cycles, all consistent"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("energy_log", help="Path to captured energy-service log output")
    parser.add_argument("body_log", help="Path to captured body-service log output")
    args = parser.parse_args()

    with open(args.energy_log) as f:
        energy_text = f.read()
    with open(args.body_log) as f:
        body_text = f.read()

    ok_energy, msg_energy = verify_energy(energy_text)
    print(f"{'PASS' if ok_energy else 'FAIL'}: energy-service: {msg_energy}")

    ok_body, msg_body = verify_body(body_text)
    print(f"{'PASS' if ok_body else 'FAIL'}: body-service: {msg_body}")

    sys.exit(0 if (ok_energy and ok_body) else 1)


if __name__ == "__main__":
    main()
