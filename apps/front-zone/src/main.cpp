// M0 walking skeleton: front-zone publisher.
//
// Publishes a HeartBeat every 200ms so the M0 spike can prove that DDS
// discovery and delivery work across the Docker bridge network (ADR-0002)
// before any real vehicle signal is modeled. See interfaces/idl/heartbeat.idl.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

#include <time.h>

#include "dds/dds.h"
#include "heartbeat.h"
#include "middleware/dds/dds_entity.hpp"

using mostrider::dds::Entity;

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
    const char* zone_id = env_or("ZONE_ID", "front-zone");
    const int period_ms = std::atoi(env_or("PUBLISH_PERIOD_MS", "200"));

    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));
    Entity topic(dds_create_topic(
        participant, &mostrider_v1_HeartBeat_desc, "HeartBeat", nullptr, nullptr));
    Entity writer(dds_create_writer(participant, topic, nullptr, nullptr));

    std::printf(
        "[%s] publisher up (CYCLONEDDS_URI=%s, period=%dms)\n",
        zone_id, env_or("CYCLONEDDS_URI", "(default)"), period_ms);
    std::fflush(stdout);

    mostrider_v1_HeartBeat sample{};
    std::strncpy(sample.zone_id, zone_id, sizeof(sample.zone_id) - 1);

    unsigned long long seq = 0;
    while (true) {
        sample.seq = seq++;
        sample.timestamp_ns = monotonic_ns();

        dds_return_t rc = dds_write(writer, &sample);
        if (rc != DDS_RETCODE_OK) {
            std::fprintf(stderr, "[%s] write failed: %s\n", zone_id, dds_strretcode(-rc));
        } else {
            std::printf(
                "[%s] published seq=%llu\n",
                zone_id, static_cast<unsigned long long>(sample.seq));
        }
        std::fflush(stdout);

        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
}
