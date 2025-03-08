#include "ads_psf/scheduler.h"
#include "ads_psf/process_context.h"
#include "ads_psf/processor_info.h"

namespace ads_psf {

Scheduler::Scheduler(std::unique_ptr<Processor> processor,
                     std::unique_ptr<AsyncExecutor> executor)
: rootProcessor_{std::move(processor)}
, executor_{std::move(executor)} {

    rootProcessor_->Init(ProcessorInfo{".", ProcessorId::Root()}, 0, *executor_);
}

ProcessStatus Scheduler::Run(DataContext& dataCtx) {
    tracker_.ScheduleEnter();
    ProcessContext processCtx{dataCtx};
    processCtx.SetTracker(&tracker_);
    ProcessStatus status = rootProcessor_->Process(processCtx);
    tracker_.ScheduleExit(status);
    return status;
}

void Scheduler::AddTracker(std::unique_ptr<ProcessTracker> tracker) {
    tracker_.AddTracker(std::move(tracker));
}

} // namespace ads_psf