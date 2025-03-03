#ifndef ADS_PSF_PROCESS_STATUS_H
#define ADS_PSF_PROCESS_STATUS_H

#include <string>
#include <ostream>

namespace ads_psf {

enum class ProcessStatus {
    OK,
    CANCELLED,
    ERROR
};

std::string ToString(ProcessStatus);

} // namespace ads_psf

std::ostream& operator<<(std::ostream&, ads_psf::ProcessStatus);

#endif // ADS_PSF_PROCESS_STATUS_H