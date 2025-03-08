#ifndef ADS_PSF_GROUP_TRACKER_H
#define ADS_PSF_GROUP_TRACKER_H

#include "ads_psf/process_tracker.h"
#include <vector>
#include <memory>

namespace ads_psf {

struct GroupTracker : ProcessTracker {
    void AddTracker(std::unique_ptr<ProcessTracker>);
    
    void ScheduleEnter() override;
    void ScheduleExit(ProcessStatus) override;
    void ProcessEnter(const ProcessorInfo&) override;
    void ProcessExit(const ProcessorInfo&, ProcessStatus) override;

private:
    std::vector<std::unique_ptr<ProcessTracker>> trackers_;
};

} // namespace ads_psf

#endif // ADS_PSF_GROUP_TRACKER_H