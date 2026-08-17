#ifndef YARR_UTILS_CONSOLE_SINK_HPP
#define YARR_UTILS_CONSOLE_SINK_HPP

#include <spdlog/sinks/base_sink.h>

#include <chrono>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>


struct ConsoleMessage
{
    spdlog::level::level_enum level;
    std::chrono::system_clock::time_point timestamp;
    std::string source;
    std::string text;
};

class ImGuiConsoleSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    // What the unnamed default logger is called in the panel. Records reach it from the free
    // functions that belong to no type, and from spdlog itself.
    static constexpr std::string_view DEFAULT_SOURCE {"app"};

    template <typename Container>
    void drain(Container& out)
    {
        std::lock_guard lock(base_sink::mutex_);
        if (pending_.empty()) {
            return;
        }

        out.insert(
            out.end(),
            std::make_move_iterator(pending_.begin()),
            std::make_move_iterator(pending_.end()));
        pending_.clear();
    }

protected:
    // base_sink::log() already holds mutex_ when calling this.
    void sink_it_(spdlog::details::log_msg const& msg) override
    {
        // Copied rather than viewed: logger_name points into the logger, and nothing here owns
        // a reference keeping that logger registered for as long as the panel holds the row.
        pending_.push_back(
            {
                .level     = msg.level,
                .timestamp = msg.time,
                .source    = (msg.logger_name.size() == 0)
                             ? std::string {DEFAULT_SOURCE}
                             : std::string {msg.logger_name.data(), msg.logger_name.size()},
                .text = std::string {msg.payload.data(), msg.payload.size()}
            });
    }

    void flush_() override
    {
        // No flushing needed for in-memory sink
    }

private:
    std::vector<ConsoleMessage> pending_;
};

#endif // YARR_UTILS_CONSOLE_SINK_HPP
