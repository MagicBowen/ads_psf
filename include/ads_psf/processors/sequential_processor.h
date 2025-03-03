#ifndef ADS_PSF_SEQUENTIAL_PROCESSOR_H
#define ADS_PSF_SEQUENTIAL_PROCESSOR_H

#include "ads_psf/processors/group_processor.h"

namespace ads_psf {

struct SequentialProcessor : GroupProcessor {
    using GroupProcessor::GroupProcessor;

private:
    ProcessStatus Execute(ProcessContext&) override;
};

} // namespace ads_psf

#endif // ADS_PSF_SEQUENTIAL_PROCESSOR_H