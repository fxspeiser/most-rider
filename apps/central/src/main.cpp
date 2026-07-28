// Central: the discovery/health hub (M0 walking skeleton, extended in M1
// with golden-run recording and in M2 with the discovery module).
//
// Central is not "just another zone" — it hosts the discovery module
// (ADR-0001, ADR-0004): it consumes every peripheral zone's HeartBeat and
// CapabilityAnnounce, tracks liveliness app-side (middleware/health/
// zone_registry.hpp — DDS liveliness/deadline QoS is scoped to M4, not
// pulled forward here), and republishes TopologyState + DiagnosticEvent so
// a single subscription gives a client the whole topology.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <time.h>

#include "dds/dds.h"
#include "heartbeat.h"
#include "discovery.h"
#include "middleware/dds/dds_entity.hpp"
#include "middleware/health/zone_registry.hpp"
#include "middleware/metrics/sample_recorder.hpp"

using mostrider::dds::Entity;
using mostrider::health::ZoneRegistry;
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

void publish_topology(dds_entity_t writer, const std::string& zone_id, const mostrider::health::ZoneInfo& info) {
    mostrider_v1_TopologyState state{};
    std::strncpy(state.zone_id, zone_id.c_str(), sizeof(state.zone_id) - 1);
    state.alive = info.alive;
    state.last_seq = info.last_seq;
    state.last_seen_ns = info.last_seen_ns;
    dds_write(writer, &state);
}

void publish_diagnostic(
    dds_entity_t writer, unsigned long long& event_id, const std::string& zone_id, bool alive) {
    mostrider_v1_DiagnosticEvent event{};
    event.event_id = event_id++;
    std::strncpy(event.zone_id, zone_id.c_str(), sizeof(event.zone_id) - 1);
    std::strncpy(event.severity, alive ? "info" : "warning", sizeof(event.severity) - 1);
    std::string message = zone_id + (alive ? " recovered" : " went stale");
    std::strncpy(event.message, message.c_str(), sizeof(event.message) - 1);
    event.timestamp_ns = monotonic_ns();
    dds_write(writer, &event);
    std::printf(
        "central: [%s] %s (event_id=%llu)\n",
        event.severity, message.c_str(), static_cast<unsigned long long>(event.event_id));
}

} // namespace

int main() {
    const long long stale_after_ns =
        static_cast<long long>(std::atoll(env_or("STALE_AFTER_MS", "1000"))) * 1'000'000LL;

    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));

    Entity hb_topic(dds_create_topic(
        participant, &mostrider_v1_HeartBeat_desc, "HeartBeat", nullptr, nullptr));
    Entity hb_reader(dds_create_reader(participant, hb_topic, nullptr, nullptr));

    Entity cap_topic(dds_create_topic(
        participant, &mostrider_v1_CapabilityAnnounce_desc, "CapabilityAnnounce", nullptr, nullptr));
    Entity cap_reader(dds_create_reader(participant, cap_topic, nullptr, nullptr));

    Entity topo_topic(dds_create_topic(
        participant, &mostrider_v1_TopologyState_desc, "TopologyState", nullptr, nullptr));
    Entity topo_writer(dds_create_writer(participant, topo_topic, nullptr, nullptr));

    Entity diag_topic(dds_create_topic(
        participant, &mostrider_v1_DiagnosticEvent_desc, "DiagnosticEvent", nullptr, nullptr));
    Entity diag_writer(dds_create_writer(participant, diag_topic, nullptr, nullptr));

    Entity waitset(dds_create_waitset(participant));
    dds_waitset_attach(waitset, hb_reader, hb_reader);
    dds_waitset_attach(waitset, cap_reader, cap_reader);

    SampleRecorder recorder(env_or("RECORD_PATH", ""));
    const long long duration_s = std::atoll(env_or("RECORD_DURATION_S", "0"));

    std::printf(
        "central: discovery hub up (CYCLONEDDS_URI=%s, stale_after_ms=%lld, recording=%s)\n",
        env_or("CYCLONEDDS_URI", "(default)"),
        stale_after_ns / 1'000'000LL,
        recorder.active() ? env_or("RECORD_PATH", "") : "off");
    std::fflush(stdout);

    ZoneRegistry registry;
    unsigned long long event_id = 0;
    unsigned long long recorded_count = 0;
    const long long run_start_ns = monotonic_ns();

    void* hb_samples[8] = {};
    dds_sample_info_t hb_infos[8];
    mostrider_v1_HeartBeat hb_storage[8];
    for (auto& s : hb_samples) {
        s = &hb_storage[&s - hb_samples];
    }

    void* cap_samples[8] = {};
    dds_sample_info_t cap_infos[8];
    mostrider_v1_CapabilityAnnounce cap_storage[8];
    for (auto& s : cap_samples) {
        s = &cap_storage[&s - cap_samples];
    }

    dds_attach_t triggered[2];

    while (true) {
        if (duration_s > 0 && (monotonic_ns() - run_start_ns) >= duration_s * 1'000'000'000LL) {
            std::printf("central: golden run complete, recorded %llu samples\n", recorded_count);
            std::fflush(stdout);
            return 0;
        }

        dds_waitset_wait(waitset, triggered, 2, DDS_MSECS(500));

        dds_return_t n = dds_take(hb_reader, hb_samples, hb_infos, 8, 8);
        for (dds_return_t i = 0; i < n; ++i) {
            if (!hb_infos[i].valid_data) {
                continue;
            }
            const auto* hb = static_cast<const mostrider_v1_HeartBeat*>(hb_samples[i]);
            const long long now = monotonic_ns();
            long long latency_us = (now - hb->timestamp_ns) / 1000;
            std::printf(
                "central: received heartbeat zone=%s seq=%llu latency_us=%lld\n",
                hb->zone_id, static_cast<unsigned long long>(hb->seq), latency_us);

            registry.on_heartbeat(hb->zone_id, hb->seq, now);

            if (recorder.active()) {
                recorder.record(hb->zone_id, hb->seq, latency_us);
                ++recorded_count;
            }
        }

        dds_return_t cn = dds_take(cap_reader, cap_samples, cap_infos, 8, 8);
        for (dds_return_t i = 0; i < cn; ++i) {
            if (!cap_infos[i].valid_data) {
                continue;
            }
            const auto* cap = static_cast<const mostrider_v1_CapabilityAnnounce*>(cap_samples[i]);
            std::printf(
                "central: capabilities zone=%s capabilities=%s api_version=%s\n",
                cap->zone_id, cap->capabilities, cap->api_version);
            registry.on_capability_announce(cap->zone_id, cap->capabilities, cap->api_version);
        }

        auto transitions = registry.evaluate(monotonic_ns(), stale_after_ns);
        for (const auto& t : transitions) {
            publish_topology(topo_writer, t.zone_id, registry.zones().at(t.zone_id));
            publish_diagnostic(diag_writer, event_id, t.zone_id, t.alive);
        }

        std::fflush(stdout);
    }
}
