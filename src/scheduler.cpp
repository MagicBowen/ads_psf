#include "ads_psf/scheduler.h"
#include "ads_psf/process_context.h"
#include "ads_psf/processor_info.h"

namespace ads_psf {

Scheduler::Scheduler(std::unique_ptr<Processor> processor,
                     std::unique_ptr<AsyncExecutor> executor)
: rootProcessor_{std::move(processor)}
, executor_{std::move(executor)} {

    rootProcessor_->Init(ProcessorInfo{"root", ProcessorId::Root()}, 0, *executor_);
}

ProcessStatus Scheduler::Run(DataContext& dataCtx) {
    ProcessContext processCtx{dataCtx};
    processCtx.SetTracker(&tracker_);
    return rootProcessor_->Process(processCtx);
}

void Scheduler::AddTracker(std::unique_ptr<ProcessTracker> tracker) {
    tracker_.AddTracker(std::move(tracker));
}

void Scheduler::Dump() const {
    tracker_.Dump();
}

} // namespace ads_psf