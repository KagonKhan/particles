#ifndef PROJECT_UTILS_UTIL_HPP
#define PROJECT_UTILS_UTIL_HPP

#include "logger.hpp"

#include <atomic>
#include <type_traits>
#include <utility>

/// @brief Inherit `MyClass : private TrackingLogger<MyClass>` to track allocations and special member functions
/// @tparam Derived The class you want to track
template <typename Derived>
class TrackingLogger : Logger<Derived>
{
protected:
    TrackingLogger()
    {
        static_assert(
            !std::is_convertible_v<Derived*, TrackingLogger<Derived>*>,
            "Inherit privately: class MyClass : private TrackingLogger<MyClass>"
        );
        instances_.fetch_add(1, std::memory_order_relaxed);
        this->TRACE("{} constructor [{}]", type_name<Derived>(), instances_.load(std::memory_order_relaxed));
    }

    ~TrackingLogger() noexcept
    {
        instances_.fetch_sub(1, std::memory_order_relaxed);
        this->TRACE("{} destructor [{}]", type_name<Derived>(), instances_.load(std::memory_order_relaxed));
    }

    TrackingLogger(TrackingLogger const& other)
        : Logger<Derived>(other)
    {
        instances_.fetch_add(1, std::memory_order_relaxed);
        this->TRACE("{} copy constructor [{}]", type_name<Derived>(), instances_.load(std::memory_order_relaxed));
    }

    TrackingLogger(TrackingLogger&& other) noexcept
        : Logger<Derived>(std::move(other))
    {
        instances_.fetch_add(1, std::memory_order_relaxed);
        this->TRACE("{} move constructor [{}]", type_name<Derived>(), instances_.load(std::memory_order_relaxed));
    }

    TrackingLogger& operator =(TrackingLogger const& other)
    {
        if (this != &other) {
            Logger<Derived>::operator =(other);
            this->TRACE("{} copy assignment [{}]", type_name<Derived>(), instances_.load(std::memory_order_relaxed));
        }

        return *this;
    }

    TrackingLogger& operator =(TrackingLogger&& other) noexcept
    {
        if (this != &other) {
            Logger<Derived>::operator =(std::move(other));
            this->TRACE("{} move assignment [{}]", type_name<Derived>(), instances_.load(std::memory_order_relaxed));
        }

        return *this;
    }

private:
    inline static std::atomic<int64_t> instances_ {0};
};


#endif // PROJECT_UTILS_UTIL_HPP
