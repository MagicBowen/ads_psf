/**
* Copyright (c) wangbo@joycode.art 2024
*/

#ifndef SCHEDULER_DSL_H
#define SCHEDULER_DSL_H

#include "ads_psf/scheduler.h"
#include "ads_psf/trackers/console_tracker.h"
#include "ads_psf/trackers/timing_tracker.h"
#include "ads_psf/executors/std_async_executor.h"
#include "ads_psf/processors/sequential_processor.h"
#include <memory>

namespace ads_psf {

template<typename PROCESSOR, typename EXECUTOR, typename ...TRACKERS>
std::unique_ptr<Scheduler> MakeScheduler(PROCESSOR && processor, EXECUTOR && executor, TRACKERS&& ...trackers) {
    auto scheduler = std::make_unique<Scheduler>(std::move(processor), std::move(executor));

    // 使用数组初始化列表辅助展开参数包
    using expander = int[];
    (void)expander{0, (scheduler->AddTracker(std::move(trackers)), 0)...};

    return scheduler;
}

template<typename TRACKER, typename ...Args>
std::unique_ptr<ProcessTracker> MakeTracker(Args&& ...args) {
    return std::make_unique<TRACKER>(std::forward<Args>(args)...);
}

template<typename EXECUTOR, typename ...Args>
std::unique_ptr<AsyncExecutor> MakeExecutor(Args&& ...args) {
    return std::make_unique<EXECUTOR>(std::forward<Args>(args)...);
}

template<typename PROCESSOR>
auto ForwardRootProcessor(PROCESSOR && processor) -> decltype(auto) {
    return processor;
}

template<typename PROCESSOR1, typename PROCESSOR2, typename ...PROCESSORS>
auto ForwardRootProcessor(PROCESSOR1 && proc1, PROCESSOR2 && proc2, PROCESSORS&& ...procs) -> decltype(auto) {
    auto processor = std::make_unique<SequentialProcessor>("root");
    processor->AddProcessor(std::forward<PROCESSOR1>(proc1));
    processor->AddProcessor(std::forward<PROCESSOR2>(proc2));

    // 使用数组初始化列表辅助展开参数包
    using expander = int[];
    (void)expander{0, (processor->AddProcessor(std::forward<PROCESSORS>(procs)), 0)...};

    return processor;
}

}

#define SCHEDULER(PROCESSOR, EXECUTOR, ...)  \
ads_psf::MakeScheduler(PROCESSOR, EXECUTOR, ##__VA_ARGS__)

#define PROCESSOR(...)                      \
ads_psf::ForwardRootProcessor(__VA_ARGS__)

#define EXECUTOR(EXECUTOR, ...)             \
ads_psf::MakeExecutor<EXECUTOR>(__VA_ARGS__)

#define TRACKER(TRACKER, ...)                 \
ads_psf::MakeTracker<TRACKER>(__VA_ARGS__)


#endif
