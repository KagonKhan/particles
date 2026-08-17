#ifndef YARR_UTILS_LOGGER_HPP
#define YARR_UTILS_LOGGER_HPP

#include "utils/meta.hpp"

// Teaches the formatter about std::chrono::duration and system_clock time points, so a
// duration logs as "12ms" instead of needing .count() and a hand-written unit at every
// call site. Include this header rather than <spdlog/spdlog.h> to get that everywhere.
#include <spdlog/fmt/chrono.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>


// Names a logger after Class, so every record carries the type that emitted it and the console
// can show it. Usable two ways, because not everything that logs has an instance to hand:
// inherit it privately and call `info(...)`, or qualify it as `Logger<Class>::info(...)` from a
// static member or a free function that belongs to Class in all but language.
template <typename Class>
class Logger
{
public:
    template <typename ... Args>
    static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().trace(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void debug(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().debug(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void info(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().info(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void warning(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().warn(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void error(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().error(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void critical(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger().critical(fmt, std::forward<Args>(args)...);
    }

private:
    // Resolved on first log rather than on construction. The console sink is attached to the
    // default logger inside main(), and a clone taken before that — by a static member, or by
    // anything constructed during static initialization — would never reach the panel.
    static spdlog::logger& logger()
    {
        static std::shared_ptr<spdlog::logger> const instance = [] {
                auto name = std::string {typeName<Class>()};

                if (std::shared_ptr<spdlog::logger> existing = spdlog::get(name)) {
                    return existing;
                }

                std::shared_ptr<spdlog::logger> created = spdlog::default_logger()->clone(name);
                spdlog::register_logger(created);

                return created;
            } ();

        return *instance;
    }
};


class Log
{
public:
    template <typename ... Args>
    static void trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.trace(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void debug(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.debug(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void info(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.info(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void warning(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.warning(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void error(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.error(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void critical(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.critical(fmt, std::forward<Args>(args)...);
    }

private:
    struct Global {};

    inline static Logger<Global> logger_;
};


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
        this->trace("{} constructor [{}]", typeName<Derived>(), instances_.load(std::memory_order_relaxed));
    }

    ~TrackingLogger() noexcept
    {
        instances_.fetch_sub(1, std::memory_order_relaxed);
        this->trace("{} destructor [{}]", typeName<Derived>(), instances_.load(std::memory_order_relaxed));
    }

    TrackingLogger(TrackingLogger const& other)
        : Logger<Derived>(other)
    {
        instances_.fetch_add(1, std::memory_order_relaxed);
        this->trace("{} copy constructor [{}]", typeName<Derived>(), instances_.load(std::memory_order_relaxed));
    }

    TrackingLogger(TrackingLogger&& other) noexcept
        : Logger<Derived>(std::move(other))
    {
        instances_.fetch_add(1, std::memory_order_relaxed);
        this->trace("{} move constructor [{}]", typeName<Derived>(), instances_.load(std::memory_order_relaxed));
    }

    TrackingLogger& operator =(TrackingLogger const& other)
    {
        if (this != &other) {
            Logger<Derived>::operator =(other);
            this->trace("{} copy assignment [{}]", typeName<Derived>(), instances_.load(std::memory_order_relaxed));
        }

        return *this;
    }

    TrackingLogger& operator =(TrackingLogger&& other) noexcept
    {
        if (this != &other) {
            Logger<Derived>::operator =(std::move(other));
            this->trace("{} move assignment [{}]", typeName<Derived>(), instances_.load(std::memory_order_relaxed));
        }

        return *this;
    }

private:
    inline static std::atomic<std::int64_t> instances_ {0};
};

#endif // YARR_UTILS_LOGGER_HPP
