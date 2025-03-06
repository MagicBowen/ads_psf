#include "ads_psf/processor_id.h"
#include <sstream>

namespace {
    static constexpr uint32_t MAX_DEPTH = 9;       // 现在可以支持9层嵌套(0-8)
    static constexpr uint32_t BITS_PER_LEVEL = 8;  // 每层使用8位，每层最多支持255个处理器
    static constexpr uint64_t LEVEL_MASK = 0xFF;   // 每层的掩码
    static constexpr uint64_t DEPTH_SHIFT = 56;    // 层数的偏移量

    uint32_t GetDepthFromValue(uint64_t value) {
        return (value >> DEPTH_SHIFT) & LEVEL_MASK;
    }
}

namespace ads_psf {

ProcessorId::ProcessorId() : value_(0) {}

ProcessorId::ProcessorId(uint64_t value) : value_(value) {}

ProcessorId ProcessorId::CreateChild(const ProcessorId& parent, uint32_t childIndex) {
    uint64_t parentValue = parent.value_;
    uint32_t depth = GetDepthFromValue(parentValue);
    
    if (depth >= MAX_DEPTH - 1) {
        return parent;
    }
    
    uint32_t newDepth = depth + 1;
    uint64_t shiftAmount = depth * BITS_PER_LEVEL;
    uint64_t newValue = parentValue | ((childIndex & LEVEL_MASK) << shiftAmount);
    
    newValue = (newValue & ~(LEVEL_MASK << DEPTH_SHIFT)) | (static_cast<uint64_t>(newDepth) << DEPTH_SHIFT);
    return ProcessorId(newValue);
}

ProcessorId ProcessorId::Root() {
    // Root 节点的ID值为 0
    return ProcessorId{0};
}

ProcessorId ProcessorId::GetParent() const {
    uint32_t depth = GetDepth();
    if (depth == 0) {
        // 根ID没有父ID
        return ProcessorId();
    }
    
    uint64_t parentValue = value_;
    uint64_t clearMask = ~(LEVEL_MASK << ((depth - 1) * BITS_PER_LEVEL));
    parentValue &= clearMask;
    
    parentValue = (parentValue & ~(LEVEL_MASK << DEPTH_SHIFT)) | (static_cast<uint64_t>(depth - 1) << DEPTH_SHIFT);
    return ProcessorId(parentValue);
}

uint32_t ProcessorId::GetDepth() const {
    return GetDepthFromValue(value_);
}

uint64_t ProcessorId::GetValue() const {
    return value_;
}

uint8_t ProcessorId::GetLevelValue(uint32_t level) const {
    if (level >= GetDepth()) {
        return 0;
    }
    
    uint32_t shift = level * BITS_PER_LEVEL;
    return (value_ >> shift) & LEVEL_MASK;
}

std::string ProcessorId::ToString() const {
    uint32_t depth = GetDepth();
    if (depth == 0) return "root";
    
    std::ostringstream oss;
    oss << "$";
    for (uint32_t i = 0; i < depth; ++i) {
        oss << "." << static_cast<int>(GetLevelValue(i));
    }
    return oss.str();
}

bool ProcessorId::operator==(const ProcessorId& other) const {
    return value_ == other.value_;
}

bool ProcessorId::operator!=(const ProcessorId& other) const {
    return value_ != other.value_;
}

bool ProcessorId::IsValid() const {
    return value_ != 0 || GetDepth() == 0;
}

} // namespace ads_psf

std::ostream& operator<<(std::ostream& os, ads_psf::ProcessorId id) {
    os << id.ToString();
    return os;
}