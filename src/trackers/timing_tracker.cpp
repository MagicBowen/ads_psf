#include "ads_psf/trackers/timing_tracker.h"
#include "ads_psf/processor_info.h"
#include <algorithm>
#include <iostream>
#include <vector>

namespace ads_psf {

void TimingTracker::ScheduleEnter() {
    std::lock_guard<std::mutex> lock(mutex_);
    timingData_.clear();
}

void TimingTracker::ScheduleExit(ProcessStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    Dump();
}

void TimingTracker::ProcessEnter(const ProcessorInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::high_resolution_clock::now();
    timingData_[info.id] = std::make_pair(now, std::chrono::nanoseconds(0));
}

void TimingTracker::ProcessExit(const ProcessorInfo& info, ProcessStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::high_resolution_clock::now();
    auto it = timingData_.find(info.id);
    if (it != timingData_.end()) {
        auto duration = now - it->second.first;
        it->second.second = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    }
}

void TimingTracker::Dump() const {
    std::cout << "\n======= Processor Timing Statistics =======\n";
    
    ProcessorId rootId = ProcessorId::Root();
    if (timingData_.find(rootId) == timingData_.end()) {
        std::cout << "No timing data available!\n";
        return;
    } 
    DumpProcessor(rootId, 0);
}

void TimingTracker::DumpProcessor(const ProcessorId& id, int level) const {
    auto timingIt = timingData_.find(id);

    if (timingIt == timingData_.end()) {
        std::cout << "No timing data found for id << " << id.ToString() << " in level " << level << "!\n";
        return;
    }

    double ms = timingIt->second.second.count() / 1'000'000.0;
    std::string indent(level * 2, ' ');
    std::cout << indent << "[" << id.ToString() << "]: " << ms << " ms\n";
    
    std::vector<ProcessorId> children;
    for (const auto& [possibleChildId, _] : timingData_) {
        if (possibleChildId.GetDepth() == id.GetDepth() + 1 && 
            possibleChildId.GetParent() == id) {
            children.push_back(possibleChildId);
        }
    }

    uint32_t childDepth = id.GetDepth();
    std::sort(children.begin(), children.end(), [childDepth](const ProcessorId& a, const ProcessorId& b) {
        return a.GetLevelValue(childDepth) < b.GetLevelValue(childDepth);
    });
    
    for (const auto& childId : children) {
        DumpProcessor(childId, level + 1);
    }
}

} // namespace ads_psf