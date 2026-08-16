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
    static constexpr float FIELD_WIDTH {100.0f};

    static constexpr char const* const OPTIONS_POPUP {"console options"};

    // In spdlog::level::level_enum order, so a selected index is the level it filters on.
    static constexpr std::array<char const*, 6> LEVEL_NAMES {
        "trace", "debug", "info", "warning", "error", "critical",
    };

public:
    explicit OutputConsole(std::shared_ptr<ImGuiConsoleSink> logSink);

    void update();
    void render();

private:
    // Only the drawing is optional: messages keep arriving and ageing out while the panel
    // is hidden, so turning it back on shows the log as it stands rather than as it was.
    Knob<bool>& panelVisible {Settings::getInstance().option<bool>("View", "Console", true)};

    std::shared_ptr<ImGuiConsoleSink> sink;

    LogBuffer   log;
    std::string scratch;

    // Wrapped rows have no height in common, which is the one thing ImGuiListClipper needs.
    // These are the running vertical offsets of every row, measured once per row and reused
    // until something invalidates them, so the rows can be clipped by hand instead.
    struct RowOffsets
    {
        std::deque<float> tops {0.0f};
        float width {0.0f};
        int decoration {-1};
        std::uint64_t firstRow {0};
        std::uint64_t generation {0};
    };

    RowOffsets wrapped;

    Knob<char const*> levelFilter {"Filter messages", LEVEL_NAMES, spdlog::level::trace};

    Knob<bool> showTimestamps {"Timestamps", false};
    Knob<bool> showLevels {"Levels", false};
    Knob<bool> wrapText {"Wrap", false};
    Knob<bool> autoScroll {"Auto-scroll", true};
    Knob<int>  maxMessages {"Max messages", 8192, 64, 1'000'000, "%d",
                            "Oldest messages are dropped once the history exceeds this.",
                            ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic
    };

    void applySettings();

    void renderToolbar();
    void renderOptions();
    void renderMessages();
    void renderWrapped();
    void renderClipped();
    void drawRow(std::size_t row);

    [[nodiscard]] std::string_view formatMessage(ConsoleMessage const& message);
    [[nodiscard]] float            syncRowOffsets(float width);
};

#endif // YARR_APP_CONSOLE_HPP
