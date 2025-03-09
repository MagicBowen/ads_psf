#ifndef ADS_PSF_PROCESS_RESULT_H
#define ADS_PSF_PROCESS_RESULT_H

#include "ads_psf/process_status.h"
#include "ads_psf/processor_id.h"
#include <ostream>

namespace ads_psf {

struct ProcessResult {
    ProcessResult(const ProcessorId& id, ProcessStatus status)
    : id(id), status(status) {}

    ProcessorId id;
    ProcessStatus status;
};

} // namespace ads_psf

std::ostream& operator<<(std::ostream&, const ads_psf::ProcessResult&);

#endif // ADS_PSF_PROCESS_RESULT_H