// M4: propulsion-monitor — subscribes to PropulsionState with the exact
// same QoS as energy-service's writer (middleware/qos/propulsion_qos.hpp),
// records one-way latency the same way central does for HeartBeat (M1),
// and reports DDS deadline-missed counts. This is the measurement
// instrument for tools/run_scenario_congestion.sh's A/B comparison.

#include <cstdio>
#include <cstdlib>

#include <time.h>

#include "dds/dds.h"
#include "vehicle.h"
#include "middleware/dds/dds_entity.hpp"
#include "middleware/metrics/sample_recorder.hpp"
#include "middleware/qos/propulsion_qos.hpp"

using mostrider::dds::Entity;
using mostrider::metrics::SampleRecorder;

namespace {

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
    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));

    auto prop_qos = mostrider::qos::make_propulsion_qos();
    Entity topic(dds_create_topic(
        participant, &mostrider_v1_PropulsionState_desc, "PropulsionState", nullptr, nullptr));
    Entity reader(dds_create_reader(participant, topic, prop_qos, nullptr));

    Entity waitset(dds_create_waitset(participant));
    dds_waitset_attach(waitset, reader, reader);

    SampleRecorder recorder(env_or("RECORD_PATH", ""));
    const long long duration_s = std::atoll(env_or("RECORD_DURATION_S", "0"));

    std::printf(
        "propulsion-monitor: up (priority_qos=%s, recording=%s, duration_s=%lld)\n",
        mostrider::qos::priority_qos_enabled() ? "on" : "off",
        recorder.active() ? env_or("RECORD_PATH", "") : "off", duration_s);
    std::fflush(stdout);

    void* samples[8] = {};
    dds_sample_info_t infos[8];
    mostrider_v1_PropulsionState storage[8];
    for (auto& s : samples) {
        s = &storage[&s - samples];
    }

    const long long run_start_ns = monotonic_ns();
    unsigned long long recorded_count = 0;
    unsigned long long local_seq = 0;
    uint32_t last_deadline_missed = 0;

    while (true) {
        if (duration_s > 0 && (monotonic_ns() - run_start_ns) >= duration_s * 1'000'000'000LL) {
            std::printf(
                "propulsion-monitor: run complete, recorded %llu samples, "
                "total deadline misses %u\n",
                recorded_count, last_deadline_missed);
            std::fflush(stdout);
            return 0;
        }

        dds_waitset_wait(waitset, nullptr, 0, DDS_SECS(1));

        dds_return_t n = dds_take(reader, samples, infos, 8, 8);
        for (dds_return_t i = 0; i < n; ++i) {
            if (!infos[i].valid_data) {
                continue;
            }
            const auto* p = static_cast<const mostrider_v1_PropulsionState*>(samples[i]);
            const long long now = monotonic_ns();
            const long long latency_us = (now - p->timestamp_ns) / 1000;

            std::printf(
                "propulsion-monitor: received seq=%llu latency_us=%lld speed=%.1fkmh\n",
                local_seq, latency_us, p->vehicle_speed_kmh);

            if (recorder.active()) {
                recorder.record(p->zone_id, local_seq, latency_us);
                ++recorded_count;
            }
            ++local_seq;
        }

        dds_requested_deadline_missed_status_t deadline_status{};
        if (dds_get_requested_deadline_missed_status(reader, &deadline_status) == DDS_RETCODE_OK) {
            if (deadline_status.total_count != last_deadline_missed) {
                std::printf(
                    "propulsion-monitor: [warning] deadline missed count now %u (+%u)\n",
                    deadline_status.total_count,
                    deadline_status.total_count - last_deadline_missed);
                last_deadline_missed = deadline_status.total_count;
            }
        }

        std::fflush(stdout);
    }
}
