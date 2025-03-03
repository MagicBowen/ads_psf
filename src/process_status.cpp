#include "ads_psf/process_status.h"

namespace ads_psf {

std::string ToString(ProcessStatus status) {
    switch (status) {
    case ProcessStatus::OK:
        return "OK";
    case ProcessStatus::CANCELLED:
        return "CANCELLED";
    case ProcessStatus::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

} // namespace ads_psf

std::ostream& operator<<(std::ostream& os, ads_psf::ProcessStatus status) {
    os << ads_psf::ToString(status);
    return os;
}