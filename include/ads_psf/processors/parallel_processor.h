#ifndef ADS_PSF_PARALLEL_PROCESSOR_H
#define ADS_PSF_PARALLEL_PROCESSOR_H

#include "ads_psf/processors/group_processor.h"
#include "ads_psf/async_result.h"

namespace ads_psf {

struct ParallelProcessor : GroupProcessor {
    using GroupProcessor::GroupProcessor;

private:
    ProcessStatus Execute(ProcessContext&) override;
};

} // namespace ads_psf

#endif // ADS_PSF_PARALLEL_PROCESSOR_H