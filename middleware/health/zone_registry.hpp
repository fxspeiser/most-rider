#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace mostrider::health {

// Per-zone liveliness tracking, driven from HeartBeat/CapabilityAnnounce
// samples and a periodic staleness check — app-level, not DDS liveliness
// QoS. DDS liveliness/deadline QoS is layer 2 of the 4-layer prioritization
// stack scoped for M4 (crosscheck/adr/0001), deliberately not pulled
// forward here: M2's job is the discovery module's data model (who's known,
// who's stale), which app-level staleness tracking answers on its own and
// more simply than getting QoS-triggered instance-liveliness callbacks
// right under time pressure.
//
// Not thread-safe by design — driven from a single poll loop in central's
// main, matching the "no premature concurrency" rule.
struct ZoneInfo {
    long long last_seen_ns = 0;
    unsigned long long last_seq = 0;
    bool alive = false;
    std::string capabilities;
    std::string api_version;
};

struct Transition {
    std::string zone_id;
    bool alive;
};

class ZoneRegistry {
public:
    void on_heartbeat(const std::string& zone_id, unsigned long long seq, long long now_ns) {
        auto& z = zones_[zone_id];
        z.last_seen_ns = now_ns;
        z.last_seq = seq;
    }

    void on_capability_announce(
        const std::string& zone_id, std::string capabilities, std::string api_version) {
        auto& z = zones_[zone_id];
        z.capabilities = std::move(capabilities);
        z.api_version = std::move(api_version);
    }

    // Call periodically. Returns the zones whose alive/stale status flipped
    // this tick, so the caller can republish TopologyState/DiagnosticEvent
    // only on real transitions rather than every tick.
    std::vector<Transition> evaluate(long long now_ns, long long stale_after_ns) {
        std::vector<Transition> transitions;
        for (auto& [zone_id, info] : zones_) {
            const bool should_be_alive =
                info.last_seen_ns > 0 && (now_ns - info.last_seen_ns) < stale_after_ns;
            if (should_be_alive != info.alive) {
                info.alive = should_be_alive;
                transitions.push_back(Transition{zone_id, should_be_alive});
            }
        }
        return transitions;
    }

    const std::unordered_map<std::string, ZoneInfo>& zones() const { return zones_; }

private:
    std::unordered_map<std::string, ZoneInfo> zones_;
};

} // namespace mostrider::health
