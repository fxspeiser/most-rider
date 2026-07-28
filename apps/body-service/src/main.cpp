// M3: body-service — the "door open -> cabin light" demo beat, the
// simplest possible demonstration of a mixed-criticality service class
// (body control) riding on the same DDS bus as propulsion/energy.
//
// Deterministic square-wave door cycle (open for kDoorOpenS, closed for
// kDoorClosedS) so the behavior is exactly repeatable run-to-run.
// Headlights are directly derived from door state — no separate control
// logic, on purpose: this is a demo beat, not a body-control ECU.

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

using mostrider::dds::Entity;

namespace {

constexpr double kDoorOpenS = 3.0;
constexpr double kDoorClosedS = 5.0;
constexpr double kCycleS = kDoorOpenS + kDoorClosedS;

long long monotonic_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<long long>(ts.tv_sec) * 1'000'000'000LL + ts.tv_nsec;
}

const char* env_or(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return v ? v : fallback;
}

} // namespace

int main() {
    const char* service_id = env_or("SERVICE_ID", "body-service");
    const int period_ms = std::atoi(env_or("PUBLISH_PERIOD_MS", "200"));

    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));
    Entity topic(dds_create_topic(
        participant, &mostrider_v1_BodyState_desc, "BodyState", nullptr, nullptr));
    Entity writer(dds_create_writer(participant, topic, nullptr, nullptr));

    std::printf(
        "[%s] up (CYCLONEDDS_URI=%s, period=%dms, door_cycle=%.0fs)\n",
        service_id, env_or("CYCLONEDDS_URI", "(default)"), period_ms, kCycleS);
    std::fflush(stdout);

    const long long start_ns = monotonic_ns();

    while (true) {
        const long long now_ns = monotonic_ns();
        const double t_s = static_cast<double>(now_ns - start_ns) / 1e9;
        const double phase_s = std::fmod(t_s, kCycleS);
        const bool door_open = phase_s < kDoorOpenS;

        mostrider_v1_BodyState state{};
        std::strncpy(state.zone_id, service_id, sizeof(state.zone_id) - 1);
        state.door_open = door_open;
        state.headlights_on = door_open; // the demo beat: light follows door
        state.timestamp_ns = now_ns;
        dds_write(writer, &state);

        std::printf(
            "[%s] t=%.1fs door_open=%s headlights_on=%s\n",
            service_id, t_s, door_open ? "true" : "false", door_open ? "true" : "false");
        std::fflush(stdout);

        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
}
