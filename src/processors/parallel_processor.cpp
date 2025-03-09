#include "ads_psf/processors/parallel_processor.h"
#include "ads_psf/async_executor.h"

namespace ads_psf {

void ParallelProcessor::Init(const ProcessorInfo& info, uint32_t childIndex, AsyncExecutor& executor) {
    GroupProcessor::Init(info, childIndex, executor);

    for (uint32_t i = 0; i < processors_.size(); ++i) {
        executor_->CreateDedicatedThread(processors_[i]->GetId());
    }
}

ProcessStatus ParallelProcessor::Execute(ProcessContext& ctx) {
    ProcessTaskIds taskIds;

    for (auto& processor : processors_) {
        bool ret = executor_->SubmitDedicated(processor->GetId(), 
            [&ctx, proc = processor.get()]() 
        {
            return proc->Process(ctx);
        });
        if (ret) {
            taskIds.push_back(processor->GetId());
        }
    }

    ProcessStatus overall = ProcessStatus::OK;
    auto results = executor_->WaitForAll(taskIds);
    for (const auto& result : results) {
        if (result.status != ProcessStatus::OK) {
            overall = result.status;
        }
    }
    return overall;
}

} // namespace ads_psf