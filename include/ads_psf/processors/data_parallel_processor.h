#ifndef ADS_PSF_DATA_PARALLEL_PROCESSOR_H
#define ADS_PSF_DATA_PARALLEL_PROCESSOR_H

#include "ads_psf/processors/data_group_processor.h"
#include "ads_psf/data_parallel_id.h"
#include "ads_psf/process_result.h"
#include <future>
#include <vector>

namespace ads_psf {

template<typename DTYPE, uint32_t N>
struct DataParallelProcessor : DataGroupProcessor<DTYPE, N> {
    using DataGroupProcessor<DTYPE, N>::DataGroupProcessor;

private:
    using DataGroupProcessor<DTYPE, N>::processors_;

    ProcessStatus Execute(ProcessContext& ctx) override {
        std::vector<std::future<ProcessResult>> futures;

        for (int i = 0; i < processors_.size(); i++) {
            futures.emplace_back(std::async(std::launch::async, [&ctx, i, processor = processors_[i].get()]() {
                data_parallel::AutoSwitchId<DTYPE> switcher(i);
                return ProcessResult(processor->GetId(), processor->Process(ctx));
            }));
        }

        ProcessStatus overall = ProcessStatus::OK;
        for (auto& fut : futures) {
            auto ret = fut.get();
            if (ret.status != ProcessStatus::OK) {
                overall = ret.status;
            }
        }
        return overall;
    }
};

} // namespace ads_psf

#endif // ADS_PSF_DATA_PARALLEL_PROCESSOR_H