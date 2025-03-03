#include "ads_psf/processors/race_processor.h"
#include "ads_psf/process_context.h"
#include <future>
#include <vector>

namespace ads_psf {

ProcessStatus RaceProcessor::Execute(ProcessContext& ctx) {
    std::vector<std::future<AsyncResult>> futures;
    std::promise<ProcessStatus> finalPromise;
    auto finalFuture = finalPromise.get_future();

    auto innerCtx = ProcessContext::CreateSubContext(ctx);

    for (auto& processor : processors_) {
        futures.emplace_back(std::async(std::launch::async, [&]() {
            ProcessStatus status = processor->Process(innerCtx);
            if (status == ProcessStatus::OK) {
                if (innerCtx.TryStop()) {
                    finalPromise.set_value(status);
                }
            }
            return AsyncResult(processor->GetId(), status);
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