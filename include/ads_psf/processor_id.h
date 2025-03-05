#ifndef ADS_PSF_PROCESSOR_ID_H
#define ADS_PSF_PROCESSOR_ID_H

#include <cstdint>
#include <string>

namespace ads_psf {

struct ProcessorId {
    ProcessorId();
    explicit ProcessorId(uint64_t);
    
    static ProcessorId CreateChild(const ProcessorId& parent, uint32_t childIndex);
    static ProcessorId Root();
    
    bool IsValid() const;
    uint64_t GetValue() const;
    std::string ToString() const;

    ProcessorId GetParent() const;
    uint32_t GetDepth() const;
    uint8_t GetLevelValue(uint32_t level) const;
    
    bool operator==(const ProcessorId& other) const;
    bool operator!=(const ProcessorId& other) const;
    
    struct Hash {
        size_t operator()(const ProcessorId& id) const {
            return std::hash<uint64_t>{}(id.GetValue());
        }
    };

private:
    uint64_t value_;
};

} // namespace ads_psf

#endif // ADS_PSF_PROCESSOR_ID_H