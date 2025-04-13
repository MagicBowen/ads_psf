#ifndef ADS_PSF_DATA_GROUP_PROCESSOR_H
#define ADS_PSF_DATA_GROUP_PROCESSOR_H

#include "ads_psf/processors/group_processor.h"
#include "ads_psf/processor_factory.h"
#include "ads_psf/processor_info.h"
#include "ads_psf/async_executor.h"
#include <functional>

namespace ads_psf {

template<typename DTYPE, uint32_t N>
struct DataGroupProcessor : GroupProcessor {
    DataGroupProcessor(const std::string& name, ProcessorFactory factory)
    : GroupProcessor(name), factory_(factory) {}

private:
    void Init(const ProcessorInfo& parentInfo, uint32_t childIndex, AsyncExecutor& executor) override {
        if (!processors_.empty()) {
            return;
        }
        Processor::Init(parentInfo, childIndex, executor);

        for (uint32_t i = 0; i < N; ++i) {
            auto processor = factory_(i);
            processor->Init(ProcessorInfo{name_, id_}, i + 1, executor);
            processors_.push_back(std::move(processor));
            executor_->CreateDedicatedThread(processors_.back()->GetId());
        }
    }

private:
    ProcessorFactory factory_;
};

} // namespace ads_psf

#endif // ADS_PSF_DATA_GROUP_PROCESSOR_H