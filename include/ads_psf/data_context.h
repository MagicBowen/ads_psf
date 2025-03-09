#ifndef ADS_PSF_DATA_CONTEXT_H
#define ADS_PSF_DATA_CONTEXT_H

#include <unordered_map>
#include <shared_mutex>
#include <typeindex>
#include <memory>

namespace ads_psf {

namespace STD {
    class Any {
        private:
            struct Base {
                virtual ~Base() = default;
                virtual std::type_index type() const = 0;
                virtual std::unique_ptr<Base> clone() const = 0;
            };
            
            template<typename T>
            struct Holder : Base {
                T value;
                
                template<typename U>
                Holder(U&& v) : value(std::forward<U>(v)) {}
                
                std::type_index type() const override {
                    return std::type_index(typeid(T));
                }
                
                std::unique_ptr<Base> clone() const override {
                    return std::unique_ptr<Base>(new Holder<T>(value));
                }
            };
            
            std::unique_ptr<Base> ptr;
            
            template<typename T>
            friend T* any_cast(Any* operand) noexcept;
            
            template<typename T>
            friend const T* any_cast(const Any* operand) noexcept;
            
        public:
            Any() noexcept = default;
            
            template<typename T, typename = typename std::enable_if<!std::is_same<typename std::decay<T>::type, Any>::value>::type>
            Any(T&& value) : ptr(new Holder<typename std::decay<T>::type>(std::forward<T>(value))) {}
            
            Any(const Any& other) : ptr(other.ptr ? other.ptr->clone() : nullptr) {}
            
            Any(Any&& other) noexcept = default;
            
            Any& operator=(const Any& rhs) {
                Any(rhs).swap(*this);
                return *this;
            }
            
            Any& operator=(Any&& rhs) noexcept {
                ptr = std::move(rhs.ptr);
                return *this;
            }
            
            template<typename T, typename = typename std::enable_if<!std::is_same<typename std::decay<T>::type, Any>::value>::type>
            Any& operator=(T&& rhs) {
                Any(std::forward<T>(rhs)).swap(*this);
                return *this;
            }
            
            void reset() noexcept {
                ptr.reset();
            }
            
            bool has_value() const noexcept {
                return ptr != nullptr;
            }
            
            std::type_index type() const {
                if (!has_value()) {
                    throw std::bad_cast();
                }
                return ptr->type();
            }
            
            void swap(Any& rhs) noexcept {
                ptr.swap(rhs.ptr);
            }
        };
        
        template<typename T>
        T* any_cast(Any* operand) noexcept {
            if (operand && operand->ptr && operand->type() == std::type_index(typeid(T))) {
                return &static_cast<Any::Holder<T>*>(operand->ptr.get())->value;
            }
            return nullptr;
        }
        
        template<typename T>
        const T* any_cast(const Any* operand) noexcept {
            return any_cast<T>(const_cast<Any*>(operand));
        }
        
        template<typename T>
        T any_cast(const Any& operand) {
            auto ptr = any_cast<typename std::remove_cv<typename std::remove_reference<T>::type>::type>(&operand);
            if (!ptr) {
                throw std::bad_cast();
            }
            return *ptr;
        }
        
        template<typename T>
        T any_cast(Any& operand) {
            auto ptr = any_cast<typename std::remove_cv<typename std::remove_reference<T>::type>::type>(&operand);
            if (!ptr) {
                throw std::bad_cast();
            }
            return *ptr;
        }
}

struct DataContext {
    template<typename T>
    T* Fetch() {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        auto it = dataRepo_.find(std::type_index(typeid(T)));
        if (it != dataRepo_.end()) {
            auto ptr = STD::any_cast<std::shared_ptr<T>>(&it->second);
            return ptr ? ptr->get() : nullptr;
        }
        return nullptr;
    }

    template<typename T, typename ...Args>
    T* Create(Args&& ...args) {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
        auto it = dataRepo_.find(std::type_index(typeid(T)));
        if (it != dataRepo_.end()) {
            return nullptr;
        }
        auto ptr = std::shared_ptr<T>(new T{std::forward<Args>(args)...});
        dataRepo_[std::type_index(typeid(T))] = ptr;
        return ptr.get();        
    }

    template<typename T, typename VISITOR>
    std::size_t Travel(VISITOR&& visitor) {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        std::size_t count = 0;
        for (auto& pair : dataRepo_) {
            auto ptr = STD::any_cast<std::shared_ptr<T>>(&pair.second);
            if (ptr) {
                std::forward<VISITOR>(visitor)(pair.first, *ptr->get());
                count++;
            }
        }
        return count;
    }

private:
    std::unordered_map<std::type_index, STD::Any> dataRepo_;
    mutable std::shared_timed_mutex mutex_;
};

} // namespace ads_psf

#endif // ADS_PSF_DATA_CONTEXT_H