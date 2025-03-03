#include "ads_psf/processors/sequential_processor.h"
#include "ads_psf/process_status.h"

namespace ads_psf {

ProcessStatus SequentialProcessor::Execute(ProcessContext& ctx) {
    for (auto& processor : processors_) {
        auto status = processor->Process(ctx);
        if (status != ProcessStatus::OK) {
            return status;
        }
    }
    return ProcessStatus::OK;
}

} // namespace ads_psf