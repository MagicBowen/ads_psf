#include "ads_psf/processors/race_processor.h"
#include "ads_psf/process_context.h"
#include "ads_psf/async_executor.h"
#include <future>
#include <vector>
#include <cassert>

namespace ads_psf {

ProcessStatus RaceProcessor::Execute(ProcessContext& ctx) {
    assert(executor_ != nullptr);

    std::vector<std::future<ProcessResult>> futures;
    std::promise<ProcessStatus> finalPromise;
    auto finalFuture = finalPromise.get_future();

    auto innerCtx = ProcessContext::CreateSubContext(ctx);

    for (auto& processor : processors_) {
        // futures.emplace_back(executor_->Submit(processor->GetId(), [&innerCtx, &finalPromise, proc = processor.get()]() {
        //     ProcessStatus status = proc->Process(innerCtx);
        futures.emplace_back(std::async(std::launch::async, [&]() {
            ProcessStatus status = processor->Process(innerCtx);
            if (status == ProcessStatus::OK) {
                if (innerCtx.TryStop()) {
                    finalPromise.set_value(status);
                }
            }
            // return status;
            return ProcessResult(processor->GetId(), status);
        }));
    }

    ProcessStatus overall = finalFuture.get();
    innerCtx.Stop();

    for (auto& fut : futures) {
        auto ret = fut.get();
    }
    return overall;
}

} // namespace ads_psf