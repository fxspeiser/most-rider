// M0 walking skeleton: central subscriber.
//
// Waits on incoming HeartBeat samples from any zone and logs one-way
// latency using CLOCK_MONOTONIC. Containers on the same Docker host share
// the kernel's monotonic clock, so this comparison is valid for a
// single-host demo (see ADR-0002 and crosscheck/adr for the citation) —
// it stops being valid the moment a second physical host enters the demo.

#include <cstdio>
#include <cstdlib>
#include <chrono>

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
    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));
    Entity topic(dds_create_topic(
        participant, &mostrider_v1_HeartBeat_desc, "HeartBeat", nullptr, nullptr));
    Entity reader(dds_create_reader(participant, topic, nullptr, nullptr));

    Entity waitset(dds_create_waitset(participant));
    dds_return_t rc = dds_waitset_attach(waitset, reader, reader);
    if (rc != DDS_RETCODE_OK) {
        std::fprintf(stderr, "central: waitset attach failed: %s\n", dds_strretcode(-rc));
        return 1;
    }

    std::printf(
        "central: subscriber up (CYCLONEDDS_URI=%s)\n", env_or("CYCLONEDDS_URI", "(default)"));
    std::fflush(stdout);

    void* samples[8] = {};
    dds_sample_info_t infos[8];
    mostrider_v1_HeartBeat storage[8];
    for (auto& s : samples) {
        s = &storage[&s - samples];
    }

    while (true) {
        dds_return_t nready = dds_waitset_wait(waitset, nullptr, 0, DDS_SECS(5));
        if (nready == 0) {
            std::printf("central: no heartbeats in the last 5s\n");
            std::fflush(stdout);
            continue;
        }

        dds_return_t n = dds_take(reader, samples, infos, 8, 8);
        for (dds_return_t i = 0; i < n; ++i) {
            if (!infos[i].valid_data) {
                continue;
            }
            const auto* hb = static_cast<const mostrider_v1_HeartBeat*>(samples[i]);
            long long latency_us = (monotonic_ns() - hb->timestamp_ns) / 1000;
            std::printf(
                "central: received heartbeat zone=%s seq=%llu latency_us=%lld\n",
                hb->zone_id, static_cast<unsigned long long>(hb->seq), latency_us);
        }
        std::fflush(stdout);
    }
}
