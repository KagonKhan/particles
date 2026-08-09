#ifndef YARR_APP_CONSOLE_HPP
#define YARR_APP_CONSOLE_HPP


#include <imgui.h>
#include <spdlog/sinks/base_sink.h>
#include <cstdint>
#include <memory>
#include <mutex>


class OutputConsole
{
private:
    static constexpr char const* const TAG {"[OutputConsole]"};

public:
    OutputConsole();

public:
    void render(ImVec2 size = ImVec2{0, 0});

private:
    enum class MessageType : std::uint8_t { debug, info, warning, error, critical, };

    struct ConsoleMessage
    {
        spdlog::level::level_enum level;
        std::chrono::system_clock::time_point timestamp;
        std::string text;
    };

    [[nodiscard]] static ImU32 messageColor(ConsoleMessage const& type);
    std::string                parseMessage(ConsoleMessage const& message);

private:
    class ImGuiConsoleSink : public spdlog::sinks::base_sink<std::mutex>
    {
    private:
        static constexpr char const* const TAG {"[ImGuiConsoleSink]"};

    public:
        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            messages_.clear();
        }

        std::vector<ConsoleMessage> messages() const
        {
            std::lock_guard lock(mutex_);
            return messages_;
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            spdlog::memory_buf_t        formatted;
            base_sink::formatter_->format(msg, formatted);

            messages_.push_back(
                {
                    .level     = msg.level,
                    .timestamp = msg.time,
                    .text      = fmt::to_string(formatted)
                });
        }

        void flush_() override
        {
            // No flushing needed for in-memory sink
        }

    private:
        mutable std::mutex          mutex_;
        std::vector<ConsoleMessage> messages_;
    };

    std::shared_ptr<ImGuiConsoleSink> sink {new ImGuiConsoleSink};

    int  selectedMessageLevel = 0;
    bool showTimestamps {false};
    bool showLevels {false};
};

// TODO: more log levels

/*
 *
 *
 *
 *
 *#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <mutex>

// Define custom levels
enum class CustomLogLevel {
    trace,
    debug,
    info,
    warn,
    error,
    critical,
    custom1,
    custom2
};

// Custom sink that handles custom levels
class CustomLevelSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Convert log level to custom levels
        std::string levelTag;
        switch (msg.level) {
            case spdlog::level::info:
                levelTag = "[INFO]";
                break;
            case spdlog::level::warn:
                levelTag = "[WARN]";
                break;
            default:
                levelTag = "[CUSTOM]";
                break;
        }

        // Print custom message
        fmt::memory_buffer formatted;
        fmt::format_to(std::back_inserter(formatted), "{} {}", levelTag, std::string(msg.payload.begin(), msg.payload.end()));
        std::cout << fmt::to_string(formatted) << std::endl;
    }

    void flush_() override {}
};

int main() {
    auto customSink = std::make_shared<CustomLevelSink>();
    auto logger = std::make_shared<spdlog::logger>("custom_logger", customSink);
    spdlog::set_default_logger(logger);

    logger->info("This is an info log");
    logger->warn("This is a warning log");

    return 0;
}

 *
 *
 *
 */

#endif // YARR_APP_CONSOLE_HPP
