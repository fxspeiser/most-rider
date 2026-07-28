// M4: load-generator — floods SensorBurst to congest the bus for the
// congestion-with-priority-survival scenario (ADR-0006). This is the
// low-priority class PropulsionState must survive being drowned out by.
//
// BEST_EFFORT + KEEP_LAST(1) is set explicitly, not left to Cyclone's
// defaults (which happen to already be this) — the point is that a
// stalled consumer of this flood must never trigger reliable
// retransmission that competes with propulsion traffic for the same link.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include <time.h>

#include "dds/dds.h"
#include "sensor_burst.h"
#include "middleware/dds/dds_entity.hpp"

using mostrider::dds::Entity;
using mostrider::dds::Qos;

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
    const char* source_id = env_or("SOURCE_ID", "load-generator");
    const double rate_hz = std::atof(env_or("RATE_HZ", "500"));
    const int payload_bytes = std::clamp(std::atoi(env_or("PAYLOAD_BYTES", "2048")), 1, 4096);

    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));

    Qos qos;
    dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
    dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

    Entity topic(dds_create_topic(
        participant, &mostrider_v1_SensorBurst_desc, "SensorBurst", nullptr, nullptr));
    Entity writer(dds_create_writer(participant, topic, qos, nullptr));

    const auto period = std::chrono::duration<double>(1.0 / rate_hz);

    std::printf(
        "[%s] up (rate=%.1fHz, payload=%dB, CYCLONEDDS_URI=%s)\n",
        source_id, rate_hz, payload_bytes, env_or("CYCLONEDDS_URI", "(default)"));
    std::fflush(stdout);

    std::vector<uint8_t> buffer(static_cast<size_t>(payload_bytes));
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = static_cast<uint8_t>(i & 0xFF); // filler pattern, not real sensor data
    }

    mostrider_v1_SensorBurst sample{};
    std::strncpy(sample.source_id, source_id, sizeof(sample.source_id) - 1);
    sample.payload._buffer = buffer.data();
    sample.payload._length = static_cast<uint32_t>(buffer.size());
    sample.payload._maximum = static_cast<uint32_t>(buffer.size());
    sample.payload._release = false; // buffer is stack/vector-owned, not DDS-owned

    unsigned long long seq = 0;
    long long next_report_ns = monotonic_ns();
    unsigned long long sent_since_report = 0;

    while (true) {
        sample.seq = seq++;
        sample.timestamp_ns = monotonic_ns();

        dds_write(writer, &sample);
        ++sent_since_report;

        const long long now = monotonic_ns();
        if (now >= next_report_ns) {
            std::printf("[%s] sent %llu samples in the last second\n", source_id, sent_since_report);
            std::fflush(stdout);
            sent_since_report = 0;
            next_report_ns = now + 1'000'000'000LL;
        }

        std::this_thread::sleep_for(period);
    }
}
