#ifndef PROJECT_UTILS_UTIL_HPP
#define PROJECT_UTILS_UTIL_HPP

#include "app/exceptions.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{

struct Time
{
    [[nodiscard]] static auto measure() noexcept { return std::chrono::high_resolution_clock::now(); }
    template <typename Duration = std::chrono::milliseconds>
    [[nodiscard]] static auto duration(auto const& t1, auto const& t2) noexcept
    {
        return std::chrono::duration_cast<Duration>(t2 - t1);
    }
};

[[nodiscard]] [[maybe_unused]]
std::string fileToString(std::filesystem::path const& path)
{
    if (!std::filesystem::exists(path)) {
        throw FileError("{} file does not exist", path.string());
    }

    try {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), {}};
    }
    catch (std::exception const& e) {
        throw FileError("{} file could not be parsed: {}", path.string(), e.what());
    }
}

} // namespace

#endif // PROJECT_UTILS_UTIL_HPP
