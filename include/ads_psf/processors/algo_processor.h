#ifndef ADS_PSF_ALGO_PROCESSOR_H
#define ADS_PSF_ALGO_PROCESSOR_H

#include "ads_psf/processor.h"
#include "ads_psf/process_status.h"
#include "ads_psf/process_context.h"

namespace ads_psf {

template<typename ALGO>
struct AlgoProcessor : Processor {
    using Processor::Processor;

private:
    void Init(const ProcessorInfo& parentInfo, uint32_t childIndex) override {
        Processor::Init(parentInfo, childIndex);
        algo_.Init();
    }

    ProcessStatus Execute(ProcessContext& ctx) override {
        algo_.Execute(ctx.GetDataContext());
        return ProcessStatus::OK;
    }

private:
    ALGO algo_;
};

} // namespace ads_psf

#endif // ADS_PSF_ALGO_PROCESSOR_H