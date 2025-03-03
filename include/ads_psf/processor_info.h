#ifndef ADS_PSF_PROCESSOR_INFO_H
#define ADS_PSF_PROCESSOR_INFO_H

#include "ads_psf/processor_id.h"
#include <string>
#include <ostream>

namespace ads_psf {

struct ProcessorInfo {
    ProcessorInfo(const std::string& name, ProcessorId id)
    : name{name}, id{id} {}

    std::string ToString() const;

    const std::string name;
    const ProcessorId id;
};

} // namespace ads_psf

std::ostream& operator<<(std::ostream& os, const ads_psf::ProcessorInfo& processInfo);

#endif // ADS_PSF_PROCESSOR_INFO_H