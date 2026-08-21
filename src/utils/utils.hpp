#ifndef YARR_UTILS_UTILS_HPP
#define YARR_UTILS_UTILS_HPP

#include "exceptions.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

struct Time
{
    [[nodiscard]] static auto measure() noexcept { return std::chrono::steady_clock::now(); }
    template <typename Duration = std::chrono::milliseconds>
    [[nodiscard]] static auto duration(auto const& t1, auto const& t2) noexcept
    {
        return std::chrono::duration_cast<Duration>(t2 - t1);
    }

    template <typename Duration = std::chrono::milliseconds, typename Fn, typename ... Args>
    [[nodiscard]] static auto execution(Fn&& fn, Args&&... args)
    {
        auto const start = measure();

        if constexpr (std::is_void_v<std::invoke_result_t<Fn, Args...>>) {
            std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
            return duration<Duration>(start, measure());
        }
        else {
            auto result = std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
            return std::pair {std::move(result), duration<Duration>(start, measure())};
        }
    }
};

struct DeltaTimeClock
{
    [[nodiscard]] float sample() noexcept
    {
        auto  now = Time::measure();
        float dt  = std::max(
            1e-6F,
            static_cast<float>(Time::duration<std::chrono::nanoseconds>(previousFrame_, now).count()) / 1e9F);

        previousFrame_      = now;
        smoothedFrameTime_ += (dt - smoothedFrameTime_) * FRAME_TIME_SMOOTHING;

        return dt;
    }

    [[nodiscard]] float get() const noexcept { return smoothedFrameTime_; }

    void reset() noexcept { previousFrame_ = Time::measure(); }

private:
    static constexpr float FRAME_TIME_SMOOTHING = 0.05F;

    std::chrono::steady_clock::time_point previousFrame_ = Time::measure();

    float smoothedFrameTime_ = 1.0F / 60.0F;
};

[[nodiscard]]
inline std::string fileToString(std::filesystem::path const& path)
{
    if (!std::filesystem::exists(path)) {
        throw FileError("{} file does not exist", path.string());
    }

    std::ifstream file(path, std::ios::binary);

    if (!file) {
        throw FileError("{} file could not be opened", path.string());
    }

    std::string contents {std::istreambuf_iterator<char>(file), {}};

    if (file.bad()) {
        throw FileError("{} file could not be read", path.string());
    }

    return contents;
}

#endif // YARR_UTILS_UTILS_HPP
