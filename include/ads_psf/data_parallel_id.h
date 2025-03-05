#ifndef ADS_PSF_DATA_PARALLEL_ID_H
#define ADS_PSF_DATA_PARALLEL_ID_H

#include <cstdint>

namespace ads_psf {

namespace data_parallel {

template<typename T>
thread_local uint32_t ID = -1;

template<typename T>
struct AutoSwitchId {
    AutoSwitchId(uint32_t id) {
        oriId_ = ID<T>;
        ID<T> = id;
    }

    ~AutoSwitchId() {
        ID<T> = oriId_;
    }

private:
    uint32_t oriId_;
};

} // namespace data_parallel

} // namespace ads_psf

#endif // ADS_PSF_DATA_PARALLEL_ID_H