#ifndef PROJECT_UTILS_LOGGER_HPP
#define PROJECT_UTILS_LOGGER_HPP

#include "meta.hpp"

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>


template <typename Class>
class Logger
{
public:
    template <typename ... Args>
    void TRACE(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->trace(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void DEBUG(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->debug(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void INFO(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->info(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void WARNING(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->warn(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void ERROR(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->error(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    void CRITICAL(spdlog::format_string_t<Args...> fmt, Args&&... args) const
    {
        logger_->critical(fmt, std::forward<Args>(args)...);
    }

    Logger()
    {
        std::call_once(
            init_flag,
            [] {
                auto name = std::string(type_name<Class>());
                logger_   = spdlog::get(name);

                if (!logger_) {
                    logger_ = defaultLogger().clone(name);
                    spdlog::register_logger(logger_);
                }
            });
    }

private:
    inline static std::shared_ptr<spdlog::logger> logger_;
    inline static std::once_flag                  init_flag;

    static spdlog::logger& defaultLogger()
    {
        static std::shared_ptr<spdlog::logger> instance = spdlog::default_logger();
        return *instance.get();
    }
};


class LOG
{
public:
    template <typename ... Args>
    static void TRACE(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.TRACE(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void DEBUG(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.DEBUG(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void INFO(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.INFO(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void WARNING(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.WARNING(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void ERROR(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.ERROR(fmt, std::forward<Args>(args)...);
    }

    template <typename ... Args>
    static void CRITICAL(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        logger_.CRITICAL(fmt, std::forward<Args>(args)...);
    }

private:
    struct GLOBAL {};

    inline static Logger<GLOBAL> logger_;
};


#endif // PROJECT_UTILS_LOGGER_HPP
