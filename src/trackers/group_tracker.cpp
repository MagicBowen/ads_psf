#include "ads_psf/trackers/group_tracker.h"

namespace ads_psf {

void GroupTracker::AddTracker(std::unique_ptr<ProcessTracker> tracker) {
    trackers_.emplace_back(std::move(tracker));
}

void GroupTracker::ScheduleEnter() {
    for (auto& tracker : trackers_) {
        tracker->ScheduleEnter();
    }    
}

void GroupTracker::ScheduleExit(ProcessStatus status) {
    for (auto& tracker : trackers_) {
        tracker->ScheduleExit(status);
    }        
}

void GroupTracker::ProcessEnter(const ProcessorInfo& info) {
    for (auto& tracker : trackers_) {
        tracker->ProcessEnter(info);
    }
}

void GroupTracker::ProcessExit(const ProcessorInfo& info, ProcessStatus status) {
    for (auto& tracker : trackers_) {
        tracker->ProcessExit(info, status);
    }
}

} // namespace ads_psf