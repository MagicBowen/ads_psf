#ifndef ADS_PSF_TIMING_TRACKER_H
#define ADS_PSF_TIMING_TRACKER_H

#include "ads_psf/process_tracker.h"
#include "ads_psf/processor_id.h"
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace ads_psf {

struct ProcessorInfo;

struct TimingTracker : ProcessTracker {
    void TrackEnter(const ProcessorInfo&) override;
    void TrackExit(const ProcessorInfo&, ProcessStatus) override;
    void Dump() const override;

private:
    void DumpProcessor(const ProcessorId&, int level) const;

private:
    mutable std::mutex mutex_;
    using TimingData = std::pair<std::chrono::high_resolution_clock::time_point, std::chrono::nanoseconds>;
    std::unordered_map<ProcessorId, TimingData, ProcessorId::Hash> timingData_;
};

} // namespace ads_psf

#endif // ADS_PSF_TIMING_TRACKER_H