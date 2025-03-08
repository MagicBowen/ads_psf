#ifndef ADS_PSF_CONSOLE_TRACKER_H
#define ADS_PSF_CONSOLE_TRACKER_H

#include "ads_psf/process_tracker.h"
#include <mutex>

namespace ads_psf {

struct ConsoleTracker : ProcessTracker {
    void ScheduleEnter() override;
    void ScheduleExit(ProcessStatus) override;    
    void ProcessEnter(const ProcessorInfo&) override;
    void ProcessExit(const ProcessorInfo&, ProcessStatus) override;

private:
    std::mutex mutex_;
};

} // namespace ads_psf

#endif // ADS_PSF_CONSOLE_TRACKER_H