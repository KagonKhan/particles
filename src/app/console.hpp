#ifndef YARR_APP_CONSOLE_HPP
#define YARR_APP_CONSOLE_HPP


#include "app/settings.hpp"
#include "utils/console_sink.hpp"
#include <imgui.h>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class OutputConsole
{
private:
    static constexpr char const* const TAG {"[OutputConsole]"};

    static constexpr int DEFAULT_MAX_MESSAGES {8192};
    static constexpr int MIN_MAX_MESSAGES {64};
    static constexpr int MAX_MAX_MESSAGES {1'000'000};

public:
    OutputConsole();

    void update();
    void render();

private:
    // Only the drawing is optional: messages keep arriving and ageing out while the panel
    // is hidden, so turning it back on shows the log as it stands rather than as it was.
    Knob<bool>& visible_ {Settings::getInstance().option<bool>("View", "Console", true)};

    std::shared_ptr<ImGuiConsoleSink> sink {std::make_shared<ImGuiConsoleSink>()};

    std::vector<ConsoleMessage> messages;
    std::vector<int>            visible;
    std::string                 scratch;

    std::size_t filteredCount {0};
    int         filteredLevel {-1};
    struct
    {
        int maxMessages {DEFAULT_MAX_MESSAGES};
        int selectedMessageLevel {spdlog::level::trace};
        bool showTimestamps {false};
        bool showLevels {false};
        bool wrapText {false};
        bool autoScroll {true};
    } settings;

    [[nodiscard]] std::string_view formatMessage(ConsoleMessage const& message);

    void renderToolbar();
    void renderMessages(ImVec2 size);
    void drawMessage(int index);

    void pullNewMessages();
    void rebuildFilterIfNeeded();
};

#endif // YARR_APP_CONSOLE_HPP
