#ifndef ADS_PSF_DATA_RACE_PROCESSOR_H
#define ADS_PSF_DATA_RACE_PROCESSOR_H

#include "ads_psf/processors/data_group_processor.h"
#include "ads_psf/data_parallel_id.h"
#include "ads_psf/process_result.h"
#include <future>
#include <vector>

namespace ads_psf {

template<typename DTYPE, uint32_t N>
struct DataRaceProcessor : DataGroupProcessor<DTYPE, N> {
    using DataGroupProcessor<DTYPE, N>::DataGroupProcessor;

private:
    using DataGroupProcessor<DTYPE, N>::processors_;

    ProcessStatus Execute(ProcessContext& ctx) override {
        std::vector<std::future<ProcessResult>> futures;
        std::promise<ProcessStatus> finalPromise;
        auto finalFuture = finalPromise.get_future();

        auto innerCtx = ProcessContext::CreateSubContext(ctx);

        for (int i = 0; i < processors_.size(); i++) {
            futures.emplace_back(std::async(std::launch::async, [&, i, processor = processors_[i].get()]() {
                data_parallel::AutoSwitchId<DTYPE> switcher(i);
                ProcessStatus status = processor->Process(innerCtx);
                if (status == ProcessStatus::OK) {
                    if (innerCtx.TryStop()) {
                        finalPromise.set_value(status);
                    }
                }
                return ProcessResult(processor->GetId(), status);
            }));
        }

        ProcessStatus overall = finalFuture.get();
        innerCtx.Stop();

        for (auto& fut : futures) {
            auto ret = fut.get();
        }
        return overall;
    }
};

} // namespace ads_psf

#endif // ADS_PSF_DATA_RACE_PROCESSOR_H