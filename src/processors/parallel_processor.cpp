#include "ads_psf/processors/parallel_processor.h"
#include "ads_psf/async_executor.h"
#include <cassert>

namespace ads_psf {

ProcessStatus ParallelProcessor::Execute(ProcessContext& ctx) {
    assert(executor_ != nullptr);

    std::vector<std::future<ProcessResult>> futures;
    for (auto& processor : processors_) {
        futures.emplace_back(
            executor_->Submit(processor->GetId(), [&ctx, proc = processor.get()]() {
                return proc->Process(ctx);
            })
        );
    }

    ProcessStatus overall = ProcessStatus::OK;
    for (auto& fut : futures) {
        auto ret = fut.get();
        if (ret.status != ProcessStatus::OK) {
            overall = ret.status;
        }
    }
    return overall;
}

} // namespace ads_psf