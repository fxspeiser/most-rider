// M3: energy-service — the powertrain/energy vertical slice (ADR-0005).
//
// A deliberately simple, deterministic, clearly-labeled ILLUSTRATIVE model
// — not a validated vehicle dynamics model. Vehicle speed is defined
// directly as a smooth periodic drive cycle (not integrated from noisy
// inputs), and torque/power are derived analytically from its time
// derivative. This keeps the simulation numerically stable and exactly
// repeatable run-to-run without tuning a physics integrator under time
// pressure — see ADR-0005 for the full rationale and the constants used.
//
// Publishes PropulsionState (speed/torque) and EnergyState (battery/power)
// on the same period. See services/propulsion/README.md and
// services/energy/README.md for the executive summary.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

#include <time.h>

#include "dds/dds.h"
#include "vehicle.h"
#include "middleware/dds/dds_entity.hpp"
#include "middleware/qos/propulsion_qos.hpp"

using mostrider::dds::Entity;

namespace {

constexpr double kCycleSpeedKmh = 60.0;   // peak speed of the drive cycle
constexpr double kCyclePeriodS = 90.0;    // one full accel/decel cycle
constexpr double kWheelRadiusM = 0.33;    // typical EV wheel radius
constexpr double kDrivelineLossFactor = 1.02; // request = delivered * loss
constexpr double kBatteryCapacityKwh = 75.0;
constexpr double kAvgConsumptionKwhPerKm = 0.18;
constexpr double kInertiaTorqueScale = 900.0; // Nm per (m/s^2) — tuned only
                                               // so numbers land in a
                                               // plausible EV range; not a
                                               // calibrated vehicle mass.
constexpr double kPi = 3.14159265358979323846;

long long monotonic_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<long long>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

const char* env_or(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return v ? v : fallback;
}

struct DriveCyclePoint {
    double speed_kmh;
    double accel_mss;   // d(speed)/dt in m/s^2, analytic — not numerically differenced
};

// Vehicle speed as a smooth sinusoid: 0 -> peak -> 0 every kCyclePeriodS.
// Acceleration is the exact derivative, computed analytically so the sign
// (accelerating vs. regen-braking) is always consistent with the speed
// curve, with no differentiation noise.
DriveCyclePoint drive_cycle(double t_s) {
    const double omega = 2.0 * kPi / kCyclePeriodS;
    const double speed_kmh = kCycleSpeedKmh * 0.5 * (1.0 - std::cos(omega * t_s));
    const double speed_mss_derivative =
        kCycleSpeedKmh * 0.5 * omega * std::sin(omega * t_s) / 3.6; // kmh/s -> m/s^2
    return {speed_kmh, speed_mss_derivative};
}

} // namespace

int main() {
    const char* service_id = env_or("SERVICE_ID", "energy-service");
    const int period_ms = std::atoi(env_or("PUBLISH_PERIOD_MS", "100"));

    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));

    auto prop_qos = mostrider::qos::make_propulsion_qos();
    Entity prop_topic(dds_create_topic(
        participant, &mostrider_v1_PropulsionState_desc, "PropulsionState", nullptr, nullptr));
    Entity prop_writer(dds_create_writer(participant, prop_topic, prop_qos, nullptr));

    Entity energy_topic(dds_create_topic(
        participant, &mostrider_v1_EnergyState_desc, "EnergyState", nullptr, nullptr));
    Entity energy_writer(dds_create_writer(participant, energy_topic, nullptr, nullptr));

    std::printf(
        "[%s] up (CYCLONEDDS_URI=%s, period=%dms, cycle_period=%.0fs, priority_qos=%s)\n",
        service_id, env_or("CYCLONEDDS_URI", "(default)"), period_ms, kCyclePeriodS,
        mostrider::qos::priority_qos_enabled() ? "on" : "off");
    std::fflush(stdout);

    const long long start_ns = monotonic_ns();
    double battery_soc_pct = 80.0; // illustrative starting charge

    while (true) {
        const long long now_ns = monotonic_ns();
        const double t_s = static_cast<double>(now_ns - start_ns) / 1e9;
        const double dt_s = static_cast<double>(period_ms) / 1000.0;

        const DriveCyclePoint point = drive_cycle(t_s);
        const double torque_delivered_nm = point.accel_mss * kInertiaTorqueScale;
        const double torque_request_nm = torque_delivered_nm * kDrivelineLossFactor;

        const double angular_velocity_rad_s = (point.speed_kmh / 3.6) / kWheelRadiusM;
        const double power_draw_kw = (torque_delivered_nm * angular_velocity_rad_s) / 1000.0;

        // power_draw_kw > 0 consumes; < 0 regenerates. Clamp SoC to [0, 100]
        // — an illustrative cap, not a real BMS charge-limiting curve.
        battery_soc_pct -= (power_draw_kw * dt_s / 3600.0) / kBatteryCapacityKwh * 100.0;
        battery_soc_pct = std::clamp(battery_soc_pct, 0.0, 100.0);

        const double range_estimate_km =
            (battery_soc_pct / 100.0) * kBatteryCapacityKwh / kAvgConsumptionKwhPerKm;

        mostrider_v1_PropulsionState prop{};
        std::strncpy(prop.zone_id, service_id, sizeof(prop.zone_id) - 1);
        prop.vehicle_speed_kmh = point.speed_kmh;
        prop.torque_request_nm = torque_request_nm;
        prop.torque_delivered_nm = torque_delivered_nm;
        prop.timestamp_ns = now_ns;
        dds_write(prop_writer, &prop);

        mostrider_v1_EnergyState energy{};
        std::strncpy(energy.zone_id, service_id, sizeof(energy.zone_id) - 1);
        energy.battery_soc_pct = battery_soc_pct;
        energy.power_draw_kw = power_draw_kw;
        energy.range_estimate_km = range_estimate_km;
        energy.timestamp_ns = now_ns;
        dds_write(energy_writer, &energy);

        std::printf(
            "[%s] t=%.1fs speed=%.1fkmh torque=%.1fNm power=%.2fkW soc=%.2f%% range=%.1fkm\n",
            service_id, t_s, point.speed_kmh, torque_delivered_nm, power_draw_kw,
            battery_soc_pct, range_estimate_km);
        std::fflush(stdout);

        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
}
