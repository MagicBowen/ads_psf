#ifndef ADS_PSF_ASYNC_RESULT_H
#define ADS_PSF_ASYNC_RESULT_H

#include "ads_psf/process_status.h"
#include "ads_psf/processor_id.h"

namespace ads_psf {

struct AsyncResult {
    AsyncResult(const ProcessorId& id, ProcessStatus status)
    : id(id), status(status) {}

    ProcessorId id;
    ProcessStatus status;
};

} // namespace ads_psf

#endif // ADS_PSF_ASYNC_RESULT_H