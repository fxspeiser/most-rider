// M5: telemetry-bridge — the DDS-facing half of the API bridge (ADR-0007).
// Subscribes to every topic a client of the REST/WebSocket API might want
// and writes an atomic JSON snapshot to a shared volume. api-bridge (Python
// + FastAPI) reads this file; it never touches DDS directly, because the
// official cyclonedds Python package has no linux/arm64 wheel (ADR-0007).
//
// Single responsibility, same as central (discovery) and propulsion-monitor
// (congestion measurement): this one only aggregates-and-serializes.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <deque>
#include <string>
#include <thread>
#include <unordered_map>

#include <time.h>

#include "dds/dds.h"
#include "discovery.h"
#include "vehicle.h"
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

struct ZoneView {
    bool alive = false;
    unsigned long long last_seq = 0;
    long long last_seen_ns = 0;
};

struct DiagnosticView {
    unsigned long long event_id;
    std::string zone_id;
    std::string severity;
    std::string message;
};

std::string json_escape(const char* s) {
    std::string out;
    for (const char* p = s; *p; ++p) {
        if (*p == '"' || *p == '\\') {
            out.push_back('\\');
        }
        out.push_back(*p);
    }
    return out;
}

// Write-then-rename so api-bridge never reads a half-written snapshot —
// the same pattern middleware/metrics/sample_recorder.hpp and every golden
// -run script in this repo already uses.
void write_snapshot_atomic(const std::string& path, const std::string& contents) {
    const std::string tmp_path = path + ".tmp";
    std::FILE* f = std::fopen(tmp_path.c_str(), "w");
    if (!f) {
        std::fprintf(stderr, "telemetry-bridge: failed to open %s\n", tmp_path.c_str());
        return;
    }
    std::fwrite(contents.data(), 1, contents.size(), f);
    std::fclose(f);
    std::rename(tmp_path.c_str(), path.c_str());
}

} // namespace

int main() {
    const char* snapshot_path = env_or("SNAPSHOT_PATH", "/data/snapshot.json");
    const int write_period_ms = std::atoi(env_or("SNAPSHOT_PERIOD_MS", "200"));

    Entity participant(dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr));

    Entity topo_topic(dds_create_topic(
        participant, &mostrider_v1_TopologyState_desc, "TopologyState", nullptr, nullptr));
    Entity topo_reader(dds_create_reader(participant, topo_topic, nullptr, nullptr));

    Entity diag_topic(dds_create_topic(
        participant, &mostrider_v1_DiagnosticEvent_desc, "DiagnosticEvent", nullptr, nullptr));
    Entity diag_reader(dds_create_reader(participant, diag_topic, nullptr, nullptr));

    Entity prop_topic(dds_create_topic(
        participant, &mostrider_v1_PropulsionState_desc, "PropulsionState", nullptr, nullptr));
    Entity prop_reader(dds_create_reader(participant, prop_topic, nullptr, nullptr));

    Entity energy_topic(dds_create_topic(
        participant, &mostrider_v1_EnergyState_desc, "EnergyState", nullptr, nullptr));
    Entity energy_reader(dds_create_reader(participant, energy_topic, nullptr, nullptr));

    Entity body_topic(dds_create_topic(
        participant, &mostrider_v1_BodyState_desc, "BodyState", nullptr, nullptr));
    Entity body_reader(dds_create_reader(participant, body_topic, nullptr, nullptr));

    Entity waitset(dds_create_waitset(participant));
    dds_waitset_attach(waitset, topo_reader, topo_reader);
    dds_waitset_attach(waitset, diag_reader, diag_reader);
    dds_waitset_attach(waitset, prop_reader, prop_reader);
    dds_waitset_attach(waitset, energy_reader, energy_reader);
    dds_waitset_attach(waitset, body_reader, body_reader);

    std::printf(
        "telemetry-bridge: up (CYCLONEDDS_URI=%s, snapshot=%s)\n",
        env_or("CYCLONEDDS_URI", "(default)"), snapshot_path);
    std::fflush(stdout);

    std::unordered_map<std::string, ZoneView> zones;
    std::deque<DiagnosticView> recent_diagnostics;
    constexpr size_t kMaxDiagnostics = 50;

    bool have_propulsion = false, have_energy = false, have_body = false;
    mostrider_v1_PropulsionState latest_prop{};
    mostrider_v1_EnergyState latest_energy{};
    mostrider_v1_BodyState latest_body{};

    void* topo_samples[8] = {};
    dds_sample_info_t topo_infos[8];
    mostrider_v1_TopologyState topo_storage[8];
    for (auto& s : topo_samples) s = &topo_storage[&s - topo_samples];

    void* diag_samples[8] = {};
    dds_sample_info_t diag_infos[8];
    mostrider_v1_DiagnosticEvent diag_storage[8];
    for (auto& s : diag_samples) s = &diag_storage[&s - diag_samples];

    void* prop_samples[4] = {};
    dds_sample_info_t prop_infos[4];
    mostrider_v1_PropulsionState prop_storage[4];
    for (auto& s : prop_samples) s = &prop_storage[&s - prop_samples];

    void* energy_samples[4] = {};
    dds_sample_info_t energy_infos[4];
    mostrider_v1_EnergyState energy_storage[4];
    for (auto& s : energy_samples) s = &energy_storage[&s - energy_samples];

    void* body_samples[4] = {};
    dds_sample_info_t body_infos[4];
    mostrider_v1_BodyState body_storage[4];
    for (auto& s : body_samples) s = &body_storage[&s - body_samples];

    long long next_write_ns = 0;

    while (true) {
        dds_waitset_wait(waitset, nullptr, 0, DDS_MSECS(200));

        dds_return_t n = dds_take(topo_reader, topo_samples, topo_infos, 8, 8);
        for (dds_return_t i = 0; i < n; ++i) {
            if (!topo_infos[i].valid_data) continue;
            const auto* t = static_cast<const mostrider_v1_TopologyState*>(topo_samples[i]);
            zones[t->zone_id] = ZoneView{t->alive, t->last_seq, t->last_seen_ns};
        }

        n = dds_take(diag_reader, diag_samples, diag_infos, 8, 8);
        for (dds_return_t i = 0; i < n; ++i) {
            if (!diag_infos[i].valid_data) continue;
            const auto* d = static_cast<const mostrider_v1_DiagnosticEvent*>(diag_samples[i]);
            recent_diagnostics.push_back(
                DiagnosticView{d->event_id, d->zone_id, d->severity, d->message});
            while (recent_diagnostics.size() > kMaxDiagnostics) {
                recent_diagnostics.pop_front();
            }
        }

        n = dds_take(prop_reader, prop_samples, prop_infos, 4, 4);
        for (dds_return_t i = 0; i < n; ++i) {
            if (!prop_infos[i].valid_data) continue;
            latest_prop = *static_cast<const mostrider_v1_PropulsionState*>(prop_samples[i]);
            have_propulsion = true;
        }

        n = dds_take(energy_reader, energy_samples, energy_infos, 4, 4);
        for (dds_return_t i = 0; i < n; ++i) {
            if (!energy_infos[i].valid_data) continue;
            latest_energy = *static_cast<const mostrider_v1_EnergyState*>(energy_samples[i]);
            have_energy = true;
        }

        n = dds_take(body_reader, body_samples, body_infos, 4, 4);
        for (dds_return_t i = 0; i < n; ++i) {
            if (!body_infos[i].valid_data) continue;
            latest_body = *static_cast<const mostrider_v1_BodyState*>(body_samples[i]);
            have_body = true;
        }

        const long long now = monotonic_ns();
        if (now < next_write_ns) {
            continue;
        }
        next_write_ns = now + static_cast<long long>(write_period_ms) * 1'000'000LL;

        std::string json = "{\n";
        json += "  \"generated_at_monotonic_ns\": " + std::to_string(now) + ",\n";

        json += "  \"zones\": {\n";
        bool first_zone = true;
        for (const auto& [zone_id, view] : zones) {
            if (!first_zone) json += ",\n";
            first_zone = false;
            json += "    \"" + json_escape(zone_id.c_str()) + "\": {";
            json += "\"alive\": " + std::string(view.alive ? "true" : "false") + ", ";
            json += "\"last_seq\": " + std::to_string(view.last_seq) + ", ";
            json += "\"last_seen_ns\": " + std::to_string(view.last_seen_ns);
            json += "}";
        }
        json += "\n  },\n";

        json += "  \"diagnostics\": [\n";
        bool first_diag = true;
        for (const auto& d : recent_diagnostics) {
            if (!first_diag) json += ",\n";
            first_diag = false;
            json += "    {\"event_id\": " + std::to_string(d.event_id) + ", ";
            json += "\"zone_id\": \"" + json_escape(d.zone_id.c_str()) + "\", ";
            json += "\"severity\": \"" + json_escape(d.severity.c_str()) + "\", ";
            json += "\"message\": \"" + json_escape(d.message.c_str()) + "\"}";
        }
        json += "\n  ],\n";

        char numbuf[256];
        json += "  \"propulsion\": ";
        if (have_propulsion) {
            std::snprintf(
                numbuf, sizeof(numbuf),
                "{\"vehicle_speed_kmh\": %.2f, \"torque_request_nm\": %.2f, "
                "\"torque_delivered_nm\": %.2f}",
                latest_prop.vehicle_speed_kmh, latest_prop.torque_request_nm,
                latest_prop.torque_delivered_nm);
            json += numbuf;
        } else {
            json += "null";
        }
        json += ",\n";

        json += "  \"energy\": ";
        if (have_energy) {
            std::snprintf(
                numbuf, sizeof(numbuf),
                "{\"battery_soc_pct\": %.2f, \"power_draw_kw\": %.2f, "
                "\"range_estimate_km\": %.1f}",
                latest_energy.battery_soc_pct, latest_energy.power_draw_kw,
                latest_energy.range_estimate_km);
            json += numbuf;
        } else {
            json += "null";
        }
        json += ",\n";

        json += "  \"body\": ";
        if (have_body) {
            json += "{\"door_open\": ";
            json += (latest_body.door_open ? "true" : "false");
            json += ", \"headlights_on\": ";
            json += (latest_body.headlights_on ? "true" : "false");
            json += "}";
        } else {
            json += "null";
        }
        json += "\n}\n";

        write_snapshot_atomic(snapshot_path, json);
    }
}
