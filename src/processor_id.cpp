#include "ads_psf/processor_id.h"
#include <sstream>

namespace {
    static constexpr uint32_t MAX_DEPTH = 8;       // 最多支持8层嵌套
    static constexpr uint32_t BITS_PER_LEVEL = 8;  // 每层使用8位，每层最多支持255个处理器
    static constexpr uint64_t LEVEL_MASK = 0xFF;   // 每层的掩码
    static constexpr uint64_t DEPTH_SHIFT = 56;    // 层数的偏移量
}

namespace ads_psf {

ProcessorId::ProcessorId() : value_(0) {}

ProcessorId::ProcessorId(uint64_t value) : value_(value) {}

ProcessorId ProcessorId::CreateChild(const ProcessorId& parent, uint32_t childIndex) {
    uint64_t parentValue = parent.value_;
    uint32_t depth = GetDepthFromValue(parentValue);
    
    if (depth >= MAX_DEPTH) {
        return parent;
    }
    
    uint32_t newDepth = depth + 1;
    uint64_t shiftAmount = (newDepth - 1) * BITS_PER_LEVEL;
    uint64_t newValue = parentValue | ((childIndex & LEVEL_MASK) << shiftAmount);
    
    newValue = (newValue & ~(LEVEL_MASK << DEPTH_SHIFT)) | (static_cast<uint64_t>(newDepth) << DEPTH_SHIFT);
    return ProcessorId(newValue);
}

ProcessorId ProcessorId::Root() {
    // Root 节点的深度为 1，ID值为 1
    return ProcessorId((static_cast<uint64_t>(1) << DEPTH_SHIFT) | 1);
}

ProcessorId ProcessorId::GetParent() const {
    uint32_t depth = GetDepth();
    if (depth <= 1) {
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
    if (depth == 0) return "null";
    
    std::ostringstream oss;
    oss << static_cast<int>(GetLevelValue(0));
    for (uint32_t i = 1; i < depth; ++i) {
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

uint32_t ProcessorId::GetDepthFromValue(uint64_t value) {
    return (value >> DEPTH_SHIFT) & LEVEL_MASK;
}

} // namespace ads_psf