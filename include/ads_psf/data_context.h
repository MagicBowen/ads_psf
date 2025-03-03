#ifndef ADS_PSF_DATA_CONTEXT_H
#define ADS_PSF_DATA_CONTEXT_H

#include <unordered_map>
#include <shared_mutex>
#include <typeindex>
#include <memory>
#include <any>

namespace ads_psf {

struct DataContext {
    template<typename T>
    T* Fetch() {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = dataRepo_.find(std::type_index(typeid(T)));
        if (it != dataRepo_.end()) {
            return std::any_cast<std::shared_ptr<T>>(it->second).get();
        }
        return nullptr;
    }

    template<typename T, typename ...Args>
    T* Create(Args&& ...args) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = dataRepo_.find(typeid(T));
        if (it != dataRepo_.end()) {
            return nullptr;
        }
        auto ptr = std::shared_ptr<T>(new T{std::forward<Args>(args)...});
        dataRepo_[std::type_index(typeid(T))] = ptr;
        return ptr.get();        
    }

    template<typename T, typename VISITOR>
    std::size_t Travel(VISITOR&& visitor) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::size_t count = 0;
        for (auto& [type, item] : dataRepo_) {
            auto dataPtr = std::any_cast<std::shared_ptr<T>>(item).get();
            std::forward<VISITOR>(visitor)(type, *dataPtr);
            count++;
        }
        return count;
    }

private:
    std::unordered_map<std::type_index, std::any> dataRepo_;
    mutable std::shared_mutex mutex_;
};

} // namespace ads_psf

#endif // ADS_PSF_DATA_CONTEXT_H