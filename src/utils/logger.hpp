#ifndef PROJECT_UTILS_LOGGER_HPP
#define PROJECT_UTILS_LOGGER_HPP

#include "meta.hpp"

// Teaches the formatter about std::chrono::duration and system_clock time points, so a
// duration logs as "12ms" instead of needing .count() and a hand-written unit at every
// call site. Include this header rather than <spdlog/spdlog.h> to get that everywhere.
#include <spdlog/fmt/chrono.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>


template <typename Class>
class Logger
{
public:
    template <typename ... Args>
    void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->trace(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->debug(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void info(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->info(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void warning(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->warn(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void error(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->error(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->critical(fmt, std::forward<Args>(args)...);
    }

    Logger()
    {
        std::call_once(
            initFlag_,
            [] {
                auto name = std::string(typeName<Class>());
                logger_   = spdlog::get(name);

                if (!logger_) {
                    logger_ = defaultLogger().clone(name);
                    spdlog::register_logger(logger_);
                }
            });
    }

private:
    inline static std::shared_ptr<spdlog::logger> logger_;
    inline static std::once_flag                  initFlag_;

    static spdlog::logger& defaultLogger()
    {
        static std::shared_ptr<spdlog::logger> instance = spdlog::default_logger();
        return *instance.get();
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
    inline static std::atomic<int64_t> instances_ {0};
};

#endif // PROJECT_UTILS_LOGGER_HPP
