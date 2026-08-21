#ifndef YARR_UTILS_LOG_BUFFER_HPP
#define YARR_UTILS_LOG_BUFFER_HPP


#include "utils/console_sink.hpp"

#include <spdlog/common.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>


class LogBuffer
{
public:
    void drain(ImGuiConsoleSink& sink);

    void setCapacity(std::size_t capacity);
    void setLevel(spdlog::level::level_enum level);
    void clear();

    [[nodiscard]] std::size_t           rowCount() const noexcept { return visible_.size(); }
    [[nodiscard]] ConsoleMessage const& row(std::size_t index) const;

    [[nodiscard]] std::uint64_t firstRow() const noexcept   { return firstRow_; }
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

private:
    void trim();
    void classify();
    void rebuild();

    std::deque<ConsoleMessage> messages_;
    std::deque<std::uint64_t>  visible_;

    std::uint64_t oldest_ {0};
    std::uint64_t classified_ {0};

    std::uint64_t firstRow_ {0};
    std::uint64_t generation_ {0};

    std::size_t               capacity_ {std::numeric_limits<std::size_t>::max()};
    spdlog::level::level_enum level_ {spdlog::level::trace};
};

#endif // YARR_UTILS_LOG_BUFFER_HPP
