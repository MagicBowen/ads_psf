#ifndef ADS_PSF_SCHEDULER_H
#define ADS_PSF_SCHEDULER_H

#include "ads_psf/processor.h"
#include "ads_psf/async_executor.h"
#include "ads_psf/trackers/group_tracker.h"
#include <memory>

namespace ads_psf {

struct DataContext;

struct Scheduler {
    Scheduler(std::unique_ptr<Processor>, 
              std::unique_ptr<AsyncExecutor>);

    ProcessStatus Run(DataContext&);
    void AddTracker(std::unique_ptr<ProcessTracker>);
    void Dump() const;

private:
    std::unique_ptr<Processor> rootProcessor_;
    std::unique_ptr<AsyncExecutor> executor_;
    GroupTracker tracker_;
};

} // namespace ads_psf

#endif // ADS_PSF_SCHEDULER_H