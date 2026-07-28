// Generic peripheral-zone runtime (M2): one binary, instantiated per zone
// via ZONE_ID/CAPABILITIES config rather than bespoke code per zone (ADR-0001
// "manifest configured, not bespoke" decision). Used for front-zone,
// rear-zone, and cabin-zone in docker-compose.yml. `central` remains its
// own binary — it hosts the discovery module, which is architecturally
// distinct (see ADR-0004), not just "one more zone."
//
// Publishes HeartBeat on a fast period (liveliness/latency signal) and
// re-announces CapabilityAnnounce on a slow period (so central catches up
// if it joins after this zone started).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

#include <time.h>

#include "dds/dds.h"
#include "heartbeat.h"
#include "discovery.h"
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
    const char* capabilities = env_or("CAPABILITIES", "telemetry");
    const int period_ms = std::atoi(env_or("PUBLISH_PERIOD_MS", "200"));
    const int announce_period_ms = std::atoi(env_or("ANNOUNCE_PERIOD_MS", "5000"));

    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));

    Entity hb_topic(dds_create_topic(
        participant, &mostrider_v1_HeartBeat_desc, "HeartBeat", nullptr, nullptr));
    Entity hb_writer(dds_create_writer(participant, hb_topic, nullptr, nullptr));

    Entity cap_topic(dds_create_topic(
        participant, &mostrider_v1_CapabilityAnnounce_desc, "CapabilityAnnounce", nullptr, nullptr));
    Entity cap_writer(dds_create_writer(participant, cap_topic, nullptr, nullptr));

    std::printf(
        "[%s] zone-runtime up (capabilities=%s, CYCLONEDDS_URI=%s, period=%dms)\n",
        zone_id, capabilities, env_or("CYCLONEDDS_URI", "(default)"), period_ms);
    std::fflush(stdout);

    mostrider_v1_HeartBeat hb{};
    std::strncpy(hb.zone_id, zone_id, sizeof(hb.zone_id) - 1);

    mostrider_v1_CapabilityAnnounce announce{};
    std::strncpy(announce.zone_id, zone_id, sizeof(announce.zone_id) - 1);
    std::strncpy(announce.capabilities, capabilities, sizeof(announce.capabilities) - 1);
    std::strncpy(announce.api_version, "v1", sizeof(announce.api_version) - 1);

    unsigned long long seq = 0;
    long long next_announce_ns = 0;

    while (true) {
        const long long now = monotonic_ns();

        if (now >= next_announce_ns) {
            dds_return_t rc = dds_write(cap_writer, &announce);
            if (rc != DDS_RETCODE_OK) {
                std::fprintf(stderr, "[%s] announce failed: %s\n", zone_id, dds_strretcode(-rc));
            } else {
                std::printf("[%s] announced capabilities=%s\n", zone_id, capabilities);
            }
            next_announce_ns = now + static_cast<long long>(announce_period_ms) * 1'000'000LL;
        }

        hb.seq = seq++;
        hb.timestamp_ns = monotonic_ns();

        dds_return_t rc = dds_write(hb_writer, &hb);
        if (rc != DDS_RETCODE_OK) {
            std::fprintf(stderr, "[%s] write failed: %s\n", zone_id, dds_strretcode(-rc));
        } else {
            std::printf(
                "[%s] published seq=%llu\n",
                zone_id, static_cast<unsigned long long>(hb.seq));
        }
        std::fflush(stdout);

        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
}
