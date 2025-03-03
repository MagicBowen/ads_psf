#ifndef ADS_PSF_PROCESSOR_FACTORY_H
#define ADS_PSF_PROCESSOR_FACTORY_H

#include "ads_psf/processors/algo_processor.h"
#include "ads_psf/processors/sequential_processor.h"
#include "ads_psf/processors/parallel_processor.h"
#include "ads_psf/processors/race_processor.h"
#include "ads_psf/processors/data_group_processor.h"
#include "ads_psf/processors/data_parallel_processor.h"
#include "ads_psf/processors/data_race_processor.h"
#include <functional>
#include <memory>

namespace ads_psf {

template<typename ALGO>
std::unique_ptr<Processor> MakeProcessor(const std::string& name) {
    return std::make_unique<AlgoProcessor<ALGO>>(name);
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

} // namespace ads_psf

#define PROCESS(ALGO)                   \
ads_psf::MakeProcessor<ALGO>(#ALGO)

#define SEQUENCE(...)                   \
ads_psf::MakeGroupProcessor<ads_psf::SequentialProcessor>("sequential", __VA_ARGS__)

#define PARALLEL(...)                   \
ads_psf::MakeGroupProcessor<ads_psf::ParallelProcessor>("parallel", __VA_ARGS__)

#define RACE(...)                       \
ads_psf::MakeGroupProcessor<ads_psf::RaceProcessor>("race", __VA_ARGS__)

#define DATA_PARALLEL(DTYPE, N, ...)    \
ads_psf::MakeDataGroupProcessor<ads_psf::DataParallelProcessor, DTYPE, N>("data_parallel", [&]() { return __VA_ARGS__; })

#define DATA_RACE(DTYPE, N, ...)        \
ads_psf::MakeDataGroupProcessor<ads_psf::DataRaceProcessor, DTYPE, N>("data_race", [&]() { return __VA_ARGS__; })

#endif // ADS_PSF_PROCESSOR_FACTORY_H