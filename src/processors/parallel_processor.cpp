#include "ads_psf/processors/parallel_processor.h"
#include <future>
#include <vector>

namespace ads_psf {

ProcessStatus ParallelProcessor::Execute(ProcessContext& ctx) {
    std::vector<std::future<ProcessResult>> futures;
    for (auto& processor : processors_) {
        futures.emplace_back(std::async(std::launch::async, [&]() {
            return ProcessResult(processor->GetId(), processor->Process(ctx));
        }));
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