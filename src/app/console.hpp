#ifndef YARR_APP_CONSOLE_HPP
#define YARR_APP_CONSOLE_HPP


#include "app/settings.hpp"
#include "utils/bases.hpp"
#include "utils/console_sink.hpp"
#include "utils/knob.hpp"
#include "utils/log_buffer.hpp"

#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>

class OutputConsole : public Immovable<OutputConsole>
{
private:
    static constexpr std::array<char const*, 6> LEVEL_NAMES {
        "trace", "debug", "info", "warning", "error", "critical",
    };

public:
    explicit OutputConsole(std::shared_ptr<ImGuiConsoleSink> log_sink)
        : sink_{std::move(log_sink)}
    {}

    void update();
    void render();

private:
    std::shared_ptr<ImGuiConsoleSink> sink_;
    LogBuffer                         logBuffer_;
    std::string                       scratchPad_;
    float                             contentWidth_ {0.0F};
    bool                              stickToBottom_ {true};

    // Row height calculations
    struct RowOffsets
    {
        std::deque<float> tops {0.0F};
        float width {0.0F};
        int decoration {-1};
        std::uint64_t firstRow {0};
        std::uint64_t generation {0};
    } wrapped_;


    Knob<bool>& panelVisible_ {Settings::getInstance().option<bool>(
                                   "View",
                                   "Console",
                                   true,
                                   "The log panel. Messages keep arriving while it is hidden.")
    };
    Knob<int> maxMessages_ {"Max messages", 8192, 64, 1'000'000, "%d",
                            "Oldest messages are dropped once the history exceeds this.",
                            ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic
    };
    Knob<char const*> levelFilter_ {"Filter messages", LEVEL_NAMES, spdlog::level::trace};
    Knob<bool>        showTimestamps_ {"Timestamps", false};
    Knob<bool>        showLevels_ {"Levels", false};
    Knob<bool>        wrapText_ {"Wrap", false};


    void applySettings();

    void renderToolbar();
    void renderMessages();
    void drawRow(std::size_t row);

    [[nodiscard]] std::string_view formatMessage(ConsoleMessage const& message);
    [[nodiscard]] float            syncRowOffsets(float width);
    [[nodiscard]] float            measureContent(std::size_t count);
};

#endif // YARR_APP_CONSOLE_HPP
