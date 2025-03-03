#include "ads_psf/processor_info.h"

namespace ads_psf {

std::string ProcessorInfo::ToString() const {
    return name + " [" + id.ToString() + "]";
}

} // namespace ads_psf

std::ostream& operator<<(std::ostream& os, const ads_psf::ProcessorInfo& processInfo) {
    os << processInfo.ToString();
    return os;
}