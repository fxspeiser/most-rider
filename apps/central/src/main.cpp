// Central subscriber (M0 walking skeleton, extended in M1 with golden-run
// recording).
//
// Waits on incoming HeartBeat samples from any zone and logs one-way
// latency using CLOCK_MONOTONIC. Containers on the same Docker host share
// the kernel's monotonic clock, so this comparison is valid for a
// single-host demo (see ADR-0002 and crosscheck/adr for the citation) —
// it stops being valid the moment a second physical host enters the demo.
//
// Set RECORD_PATH to capture raw (zone, seq, latency_us) samples as JSONL
// for tools/analyze_run.py, and RECORD_DURATION_S to auto-exit after a
// fixed window — this is what tools/run_golden_benchmark.sh drives.

#include <cstdio>
#include <cstdlib>
#include <chrono>

#include <time.h>

#include "dds/dds.h"
#include "heartbeat.h"
#include "middleware/dds/dds_entity.hpp"
#include "middleware/metrics/sample_recorder.hpp"

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
    Entity topic(dds_create_topic(
        participant, &mostrider_v1_HeartBeat_desc, "HeartBeat", nullptr, nullptr));
    Entity reader(dds_create_reader(participant, topic, nullptr, nullptr));

    Entity waitset(dds_create_waitset(participant));
    dds_return_t rc = dds_waitset_attach(waitset, reader, reader);
    if (rc != DDS_RETCODE_OK) {
        std::fprintf(stderr, "central: waitset attach failed: %s\n", dds_strretcode(-rc));
        return 1;
    }

    SampleRecorder recorder(env_or("RECORD_PATH", ""));
    const long long duration_s = std::atoll(env_or("RECORD_DURATION_S", "0"));

    std::printf(
        "central: subscriber up (CYCLONEDDS_URI=%s, recording=%s, duration_s=%lld)\n",
        env_or("CYCLONEDDS_URI", "(default)"),
        recorder.active() ? env_or("RECORD_PATH", "") : "off",
        duration_s);
    std::fflush(stdout);

    void* samples[8] = {};
    dds_sample_info_t infos[8];
    mostrider_v1_HeartBeat storage[8];
    for (auto& s : samples) {
        s = &storage[&s - samples];
    }

    const long long run_start_ns = monotonic_ns();
    unsigned long long recorded_count = 0;

    while (true) {
        if (duration_s > 0 && (monotonic_ns() - run_start_ns) >= duration_s * 1'000'000'000LL) {
            std::printf("central: golden run complete, recorded %llu samples\n", recorded_count);
            std::fflush(stdout);
            return 0;
        }

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

            if (recorder.active()) {
                recorder.record(hb->zone_id, hb->seq, latency_us);
                ++recorded_count;
            }
        }
        std::fflush(stdout);
    }
}
