#ifndef YARR_EXCEPTIONS_HPP
#define YARR_EXCEPTIONS_HPP

#include <fmt/format.h>

#include <stdexcept>

struct InitializationError : std::runtime_error
{
    template <typename ... Args>
    InitializationError(fmt::format_string<Args...> fmt, Args&&... args)
        : std::runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {}
};

struct FileError : std::runtime_error
{
    template <typename ... Args>
    FileError(fmt::format_string<Args...> fmt, Args&&... args)
        : std::runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {}
};


struct ShaderError : std::runtime_error
{
    template <typename ... Args>
    ShaderError(fmt::format_string<Args...> fmt, Args&&... args)
        : std::runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {}
};

#endif // YARR_EXCEPTIONS_HPP
