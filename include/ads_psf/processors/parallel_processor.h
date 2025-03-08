#ifndef ADS_PSF_PARALLEL_PROCESSOR_H
#define ADS_PSF_PARALLEL_PROCESSOR_H

#include "ads_psf/processors/group_processor.h"
#include "ads_psf/process_result.h"

namespace ads_psf {

struct ParallelProcessor : GroupProcessor {
    using GroupProcessor::GroupProcessor;

private:
    void Init(const ProcessorInfo&, uint32_t childIndex, AsyncExecutor&) override;
    ProcessStatus Execute(ProcessContext&) override;
};

} // namespace ads_psf

#endif // ADS_PSF_PARALLEL_PROCESSOR_H