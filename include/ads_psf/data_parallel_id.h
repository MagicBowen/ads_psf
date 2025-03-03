#ifndef ADS_PSF_DATA_PARALLEL_ID_H
#define ADS_PSF_DATA_PARALLEL_ID_H

#include <cstdint>

namespace ads_psf {

template<typename T>
thread_local uint32_t PARALLEL_ID = -1;

template<typename T>
uint32_t GetParallelId() {
    return PARALLEL_ID<T>;
}

template<typename T>
struct AutoSwitchParallelId {
    AutoSwitchParallelId(uint32_t id) {
        oriId_ = PARALLEL_ID<T>;
        PARALLEL_ID<T> = id;
    }

    ~AutoSwitchParallelId() {
        PARALLEL_ID<T> = oriId_;
    }

private:
    uint32_t oriId_;
};

} // namespace ads_psf

#endif // ADS_PSF_DATA_PARALLEL_ID_H