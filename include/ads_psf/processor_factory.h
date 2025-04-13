#include "ads_psf/processor.h"
#include <functional>

namespace ads_psf {

using ProcessorAlgoRefId = uint32_t;

using ProcessorFactory = std::function<std::unique_ptr<Processor>(ProcessorAlgoRefId)>;

} // namespace ads_psf