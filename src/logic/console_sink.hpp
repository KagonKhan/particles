#ifndef YARR_LOGIC_CONSOLE_SINK_HPP
#define YARR_LOGIC_CONSOLE_SINK_HPP

#include <spdlog/sinks/base_sink.h>

#include <string>


struct ConsoleMessage
{
    spdlog::level::level_enum level;
    std::chrono::system_clock::time_point timestamp;
    std::string text;
};

class ImGuiConsoleSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    void drain(std::vector<ConsoleMessage>& out)
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
        pending_.push_back(
            {
                .level     = msg.level,
                .timestamp = msg.time,
                .text      = std::string {msg.payload.data(), msg.payload.size()}
            });
    }

    void flush_() override
    {
        // No flushing needed for in-memory sink
    }

private:
    std::vector<ConsoleMessage> pending_;
};

#endif // YARR_LOGIC_CONSOLE_SINK_HPP
