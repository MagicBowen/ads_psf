#include "ads_psf/processors/race_processor.h"
#include "ads_psf/process_context.h"
#include "ads_psf/async_executor.h"
#include <future>

namespace ads_psf {

void RaceProcessor::Init(const ProcessorInfo& info, uint32_t childIndex, AsyncExecutor& executor) {
    GroupProcessor::Init(info, childIndex, executor);

    for (uint32_t i = 0; i < processors_.size(); ++i) {
        executor_->CreateDedicatedThread(processors_[i]->GetId());
    }
}

ProcessStatus RaceProcessor::Execute(ProcessContext& ctx) {
    ProcessTaskIds taskIds;

    std::promise<ProcessStatus> finalPromise;
    auto finalFuture = finalPromise.get_future();

    auto innerCtx = ProcessContext::CreateSubContext(ctx);

    for (auto& processor : processors_) {
        bool ret = executor_->Submit(processor->GetId(), [&innerCtx, &finalPromise, proc = processor.get()]() {
            ProcessStatus status = proc->Process(innerCtx);
            if (status == ProcessStatus::OK) {
                if (innerCtx.TryStop()) {
                    finalPromise.set_value(status);
                }
            }
            return status;
        });
        if (ret) {
            taskIds.push_back(processor->GetId());
        }
    }

    ProcessStatus overall = finalFuture.get();
    innerCtx.Stop();

    (void)executor_->WaitForAll(taskIds);
    return overall;
}

} // namespace ads_psf