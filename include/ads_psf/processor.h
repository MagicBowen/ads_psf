#ifndef ADS_PSF_PROCESSOR_H
#define ADS_PSF_PROCESSOR_H

#include "ads_psf/processor_id.h"
#include <string>

namespace ads_psf {

struct ProcessContext;
struct ProcessorInfo;
enum class ProcessStatus;

struct Processor {
    Processor(const std::string& name)
    : name_(name) {}
        
    virtual ~Processor() = default;

    virtual void Init(const ProcessorInfo& parentInfo, uint32_t childIndex);
    ProcessStatus Process(ProcessContext& ctx);

    const std::string& GetName() const {
        return name_;
    }
    
    ProcessorId GetId() const {
        return id_;
    }

private:
    virtual ProcessStatus Execute(ProcessContext&) = 0;

protected:
    std::string name_;
    ProcessorId id_{0};
};

} // namespace ads_psf

#endif // ADS_PSF_PROCESSOR_H