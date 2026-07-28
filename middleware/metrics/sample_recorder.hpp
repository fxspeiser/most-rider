#pragma once

#include <cstdio>

namespace mostrider::metrics {

// Records raw (zone, seq, latency_us) samples as JSONL for offline
// percentile analysis (tools/analyze_run.py).
//
// Raw capture, not HDR-style bucketed histograms: golden runs at this stage
// are short (seconds to low minutes) so storing every sample and computing
// exact percentiles is both simpler to get right and more honest than an
// approximated bucket scheme — the panel's "publish raw distributions"
// recommendation, taken literally. Revisit for M4+ if a scenario's duration
// or rate makes raw storage impractical (e.g. a multi-hour soak).
class SampleRecorder {
public:
    explicit SampleRecorder(const char* path) {
        if (path && path[0] != '\0') {
            file_ = std::fopen(path, "w");
        }
    }

    ~SampleRecorder() {
        if (file_) {
            std::fclose(file_);
        }
    }

    SampleRecorder(const SampleRecorder&) = delete;
    SampleRecorder& operator=(const SampleRecorder&) = delete;

    bool active() const { return file_ != nullptr; }

    void record(const char* zone_id, unsigned long long seq, long long latency_us) {
        if (!file_) {
            return;
        }
        std::fprintf(
            file_, "{\"zone\":\"%s\",\"seq\":%llu,\"latency_us\":%lld}\n",
            zone_id, seq, latency_us);
        std::fflush(file_);
    }

private:
    std::FILE* file_ = nullptr;
};

} // namespace mostrider::metrics
