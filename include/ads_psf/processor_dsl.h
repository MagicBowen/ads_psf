#ifndef ADS_PSF_PROCESSOR_FACTORY_H
#define ADS_PSF_PROCESSOR_FACTORY_H

#include "ads_psf/processor.h"
#include "ads_psf/scheduler.h"
#include "ads_psf/async_executor.h"
#include "ads_psf/process_tracker.h"
#include "ads_psf/processor_factory.h"
#include "ads_psf/processors/algo_processor.h"
#include <memory>

namespace ads_psf {

template<typename ALGO>
std::unique_ptr<Processor> MakeAlgoProcessor(const std::string& name) {
    return std::make_unique<AlgoProcessor<ALGO>>(name);
}

template<typename ALGO>
std::unique_ptr<Processor> MakeAlgoProcessor(const std::string& name, ALGO& algo) {
    return std::make_unique<AlgoProcessorRef<ALGO>>(name, algo);
}

template<typename GROUP_PROCESSOR, typename ...PROCESSORS>
std::unique_ptr<Processor> MakeGroupProcessor(const std::string& name, PROCESSORS&& ...processors) {
    auto processor = std::make_unique<GROUP_PROCESSOR>(name);
    (processor->AddProcessor(std::forward<PROCESSORS>(processors)), ...);
    return processor;
}

template<template <typename, uint32_t> class DATA_PROCESSOR, typename DTYPE, uint32_t N>
std::unique_ptr<Processor> MakeDataGroupProcessor(const std::string& name, ProcessorFactory factory) {
    return std::make_unique<DATA_PROCESSOR<DTYPE, N>>(name, factory);
}

template<typename PROCESSOR, typename EXECUTOR, typename ...TRACKERS>
std::unique_ptr<Scheduler> MakeScheduler(PROCESSOR && processor, EXECUTOR && executor, TRACKERS&& ...trackers) {
    auto scheduler = std::make_unique<Scheduler>(std::forward<PROCESSOR>(processor), std::forward<EXECUTOR>(executor));
    (scheduler->AddTracker(std::forward<TRACKERS>(trackers)), ...);
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

} // namespace ads_psf

#define SCHEDULE(PROCESSOR, EXECUTOR, ...)  \
ads_psf::MakeScheduler(std::move(PROCESSOR), std::move(EXECUTOR), ##__VA_ARGS__)

#define EXECUTOR(EXECUTOR, ...)             \
ads_psf::MakeExecutor<EXECUTOR>(##__VA_ARGS__)

#define TRACK(TRACKER, ...)                 \
ads_psf::MakeTracker<TRACKER>(##__VA_ARGS__)

#define PROCESS(ALGO)                       \
ads_psf::MakeAlgoProcessor<ALGO>(#ALGO)

#define PROCESS_REF(ALGO_INST)              \
ads_psf::MakeAlgoProcessor(#ALGO_INST, ALGO_INST)

#define SEQUENCE(...)                       \
ads_psf::MakeGroupProcessor<ads_psf::SequentialProcessor>("sequential", __VA_ARGS__)

#define PARALLEL(...)                       \
ads_psf::MakeGroupProcessor<ads_psf::ParallelProcessor>("parallel", __VA_ARGS__)

#define RACE(...)                           \
ads_psf::MakeGroupProcessor<ads_psf::RaceProcessor>("race", __VA_ARGS__)

#define DATA_PARALLEL(DTYPE, N, ...)        \
ads_psf::MakeDataGroupProcessor<ads_psf::DataParallelProcessor, DTYPE, N>("data_parallel", [&]() { return __VA_ARGS__; })

#define DATA_RACE(DTYPE, N, ...)            \
ads_psf::MakeDataGroupProcessor<ads_psf::DataRaceProcessor, DTYPE, N>("data_race", [&]() { return __VA_ARGS__; })

#endif // ADS_PSF_PROCESSOR_FACTORY_H