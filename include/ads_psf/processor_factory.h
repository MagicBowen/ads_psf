#include "ads_psf/processor.h"
#include <functional>

namespace ads_psf {

using ProcessorFactory = std::function<std::unique_ptr<Processor>()>;

} // namespace ads_psf