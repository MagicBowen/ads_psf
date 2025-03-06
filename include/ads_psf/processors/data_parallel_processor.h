#ifndef ADS_PSF_DATA_PARALLEL_PROCESSOR_H
#define ADS_PSF_DATA_PARALLEL_PROCESSOR_H

#include "ads_psf/processors/data_group_processor.h"
#include "ads_psf/data_parallel_id.h"
#include "ads_psf/async_executor.h"
#include <cassert>

namespace ads_psf {

template<typename DTYPE, uint32_t N>
struct DataParallelProcessor : DataGroupProcessor<DTYPE, N> {
    using DataGroupProcessor<DTYPE, N>::DataGroupProcessor;

private:
    using DataGroupProcessor<DTYPE, N>::processors_;
    using DataGroupProcessor<DTYPE, N>::executor_;

    ProcessStatus Execute(ProcessContext& ctx) override {
        assert(executor_ != nullptr);

        std::vector<std::future<ProcessResult>> futures;

        for (int i = 0; i < processors_.size(); i++) {
            futures.emplace_back(
                executor_->Submit(processors_[i]->GetId(), [&ctx, i, proc = processors_[i].get()]() {
                    data_parallel::AutoSwitchId<DTYPE> switcher(i);
                    return proc->Process(ctx);
                })
            );
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