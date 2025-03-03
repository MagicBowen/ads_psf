#ifndef ADS_PSF_GROUP_PROCESSOR_H
#define ADS_PSF_GROUP_PROCESSOR_H

#include "ads_psf/processor.h"
#include <vector>
#include <memory>

namespace ads_psf {

struct GroupProcessor : Processor {
    using Processor::Processor;

    void AddProcessor(std::unique_ptr<Processor>);

private:
    void Init(const ProcessorInfo&, uint32_t childIndex) override;

protected:
    std::vector<std::unique_ptr<Processor>> processors_;
};

} // namespace ads_psf

#endif // ADS_PSF_GROUP_PROCESSOR_H