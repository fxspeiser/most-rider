#pragma once

#include <cstdlib>
#include <cstring>

#include "dds/dds.h"
#include "middleware/dds/dds_entity.hpp"

namespace mostrider::qos {

// Shared QoS for PropulsionState — used identically by energy-service
// (writer) and propulsion-monitor (reader) so the two always match. QoS
// compatibility is directional (offered >= requested); building both sides
// from the same function removes any chance of the writer and reader
// drifting apart. See ADR-0006 for the layer-1/layer-2 priority-stack scope
// this implements.
//
// Toggled by ENABLE_PRIORITY_QOS so tools/run_scenario_congestion.sh can A/B
// the same code path with the priority class on or off:
//   - enabled:  RELIABLE, KEEP_LAST(1), 150ms deadline, MANUAL_BY_TOPIC
//               liveliness (2s lease), high transport_priority.
//   - disabled: BEST_EFFORT, KEEP_LAST(1) — the same class as SensorBurst,
//               the deliberately-unprivileged flood traffic.
inline bool priority_qos_enabled() {
    const char* v = std::getenv("ENABLE_PRIORITY_QOS");
    return !v || std::strcmp(v, "false") != 0;
}

inline mostrider::dds::Qos make_propulsion_qos() {
    mostrider::dds::Qos qos;
    if (priority_qos_enabled()) {
        dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
        dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);
        dds_qset_deadline(qos, DDS_MSECS(150));
        dds_qset_liveliness(qos, DDS_LIVELINESS_MANUAL_BY_TOPIC, DDS_SECS(2));
        dds_qset_transport_priority(qos, 100); // advisory only — see ADR-0006
    } else {
        dds_qset_reliability(qos, DDS_RELIABILITY_BEST_EFFORT, 0);
        dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);
    }
    return qos;
}

} // namespace mostrider::qos
