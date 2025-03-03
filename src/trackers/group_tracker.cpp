#include "ads_psf/trackers/group_tracker.h"

namespace ads_psf {

void GroupTracker::AddTracker(std::unique_ptr<ProcessTracker> tracker) {
    trackers_.emplace_back(std::move(tracker));
}

void GroupTracker::Dump() const {
    for (auto& tracker : trackers_) {
        tracker->Dump();
    }
}

void GroupTracker::TrackEnter(const ProcessorInfo& info) {
    for (auto& tracker : trackers_) {
        tracker->TrackEnter(info);
    }
}

void GroupTracker::TrackExit(const ProcessorInfo& info, ProcessStatus status) {
    for (auto& tracker : trackers_) {
        tracker->TrackExit(info, status);
    }
}

} // namespace ads_psf