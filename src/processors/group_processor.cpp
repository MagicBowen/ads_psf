#include "ads_psf/processors/group_processor.h"
#include "ads_psf/processor_info.h"

namespace ads_psf {

void GroupProcessor::AddProcessor(std::unique_ptr<Processor> processor) {
    processors_.emplace_back(std::move(processor));
}

void GroupProcessor::Init(const ProcessorInfo& parentInfo, uint32_t childIndex, AsyncExecutor& executor) {
    Processor::Init(parentInfo, childIndex, executor);

    for (uint32_t i = 0; i < processors_.size(); ++i) {
        processors_[i]->Init(ProcessorInfo{name_, id_}, i + 1, executor);
    }
}

} // namespace ads_psf