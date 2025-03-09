#include "ads_psf/process_result.h"

std::ostream& operator<<(std::ostream& os, const ads_psf::ProcessResult& result) {
    os << result.id << ": " << result.status;
    return os;
}