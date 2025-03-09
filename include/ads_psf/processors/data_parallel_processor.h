#ifndef ADS_PSF_DATA_PARALLEL_PROCESSOR_H
#define ADS_PSF_DATA_PARALLEL_PROCESSOR_H

#include "ads_psf/processors/data_group_processor.h"
#include "ads_psf/data_parallel_id.h"

namespace ads_psf {

template<typename DTYPE, uint32_t N>
struct DataParallelProcessor : DataGroupProcessor<DTYPE, N> {
    using DataGroupProcessor<DTYPE, N>::DataGroupProcessor;

private:
    using DataGroupProcessor<DTYPE, N>::processors_;
    using DataGroupProcessor<DTYPE, N>::executor_;

    ProcessStatus Execute(ProcessContext& ctx) override {
        ProcessTaskIds taskIds;

        for (int i = 0; i < processors_.size(); i++) {
            bool ret = executor_->SubmitDedicated(processors_[i]->GetId(), 
                [&ctx, i, proc = processors_[i].get()]() 
            {
                data_parallel::AutoSwitchId<DTYPE> switcher(i);
                return proc->Process(ctx);
            });
            if (ret) {
                taskIds.push_back(processors_[i]->GetId());
            }
        }

        ProcessStatus overall = ProcessStatus::OK;
        auto results = executor_->WaitForAll(taskIds);
        for (const auto& result : results) {
            if (result.status != ProcessStatus::OK) {
                overall = result.status;
            }
        }
        return overall;
    }
};

} // namespace ads_psf

#endif // ADS_PSF_DATA_PARALLEL_PROCESSOR_H