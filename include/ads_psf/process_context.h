#ifndef ADS_PSF_PROCESS_CONTEXT_H
#define ADS_PSF_PROCESS_CONTEXT_H

#include "ads_psf/process_tracker.h"
#include <atomic>

namespace ads_psf {

struct DataContext;
struct ProcessorInfo;
enum class ProcessStatus;

struct ProcessContext {
    static ProcessContext CreateSubContext(ProcessContext& parentCtx) {
        return std::move(ProcessContext{parentCtx.GetDataContext(), &parentCtx.stopFlag_, parentCtx.tracker_});
    }
    
    explicit ProcessContext(DataContext& dataCtx)
    : dataCtx_(dataCtx) {}

    ProcessContext(ProcessContext&& other)
        : dataCtx_(other.dataCtx_),
          parentStopFlag_(other.parentStopFlag_),
          tracker_(other.tracker_)
    {
        stopFlag_.store(other.stopFlag_.load());
    }

    DataContext& GetDataContext() {
        return dataCtx_;
    }
    
    void Stop() {
        stopFlag_.store(true);
    }
    
    void Resume() {
        stopFlag_.store(false);
    }
    
    bool TryStop() {
        bool expected = false;
        return stopFlag_.compare_exchange_strong(expected, true);
    }
    
    bool IsStopped() const {
        if (parentStopFlag_ && parentStopFlag_->load()) {
            return true;
        }
        return stopFlag_.load();
    }
    
    void SetTracker(ProcessTracker* tracker) {
        tracker_ = tracker;
    }
    
    void EnterProcess(const ProcessorInfo& info) {
        if (tracker_) {
            tracker_->ProcessEnter(info);
        }
    }
    
    void ExitProcess(const ProcessorInfo& info, ProcessStatus status) {
        if (tracker_) {
            tracker_->ProcessExit(info, status);
        }
    }
private:
    ProcessContext(DataContext& dataCtx, 
        const std::atomic<bool>* parentStopFlag, 
        ProcessTracker* tracker)
    : dataCtx_(dataCtx), parentStopFlag_(parentStopFlag), tracker_(tracker) {
    }

private:
    DataContext& dataCtx_;
    std::atomic<bool> stopFlag_{false};
    const std::atomic<bool>* parentStopFlag_{nullptr};
    ProcessTracker* tracker_{nullptr};
};

} // namespace ads_psf

#endif // ADS_PSF_PROCESS_CONTEXT_H