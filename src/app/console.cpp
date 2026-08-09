#include "app/console.hpp"
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <unordered_map>


OutputConsole::OutputConsole()
{
    spdlog::default_logger()->sinks().push_back(sink);
    //(void) Keyboard::AddKeybind(Keyboard::KEY::S, [] { spam = !spam; });
}

void OutputConsole::render(ImVec2 size)
{
    ImGui::SeparatorText("Console");
    if (ImGui::Button("clear")) {
        sink->clear();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Filter messages", &selectedMessageLevel, "debug\0info\0warning\0error\0critical\0\0");
    ImGui::SameLine();
    ImGui::Checkbox("Show timestamps?", &showTimestamps);
    ImGui::SameLine();
    // TODO: implement, or add a different sink that already parses.
    ImGui::Checkbox("Show levels?", &showLevels);

    // TODO: async loggers? MT?
    ImGui::BeginChild(TAG, size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        for (ConsoleMessage const& message : sink->messages()) {
            if (message.level < selectedMessageLevel) {
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, messageColor(message));

            // TODO: maybe remove [level] tags since the entire message is color-coded?
            ImGui::TextWrapped(parseMessage(message).c_str());

            ImGui::PopStyleColor();
        }

        static bool autoScroll = true;
        if (autoScroll && (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

ImU32 OutputConsole::messageColor(ConsoleMessage const& type)
{
    const static ImU32 debug_color    = IM_COL32(140, 140, 140, 255);
    const static ImU32 info_color     = IM_COL32(112, 179, 123, 255);
    const static ImU32 warn_color     = IM_COL32(193, 102, 1, 255);
    const static ImU32 error_color    = IM_COL32(198, 0, 3, 255);
    const static ImU32 critical_color = IM_COL32(198, 0, 129, 255);

    static constexpr std::array<ImU32, 6> LevelToColor {
        debug_color,
        info_color,
        warn_color,
        error_color,
        critical_color,
    };

    return LevelToColor[static_cast<std::size_t>(type.level)];
}

std::string OutputConsole::parseMessage(ConsoleMessage const& message)
{
    fmt::memory_buffer buffer;

    if (showTimestamps) {
        const auto time = std::chrono::system_clock::to_time_t(message.timestamp);

        std::tm local_time {};
#ifdef _WIN32
        localtime_s(&local_time, &time);
#else
        localtime_r(&time, &local_time);
#endif

        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()) %
            std::chrono::seconds {1};

        fmt::format_to(
            std::back_inserter(buffer),
            "{:%H:%M:%S}.{:03} ",
            local_time,
            milliseconds.count());
    }


    if (showLevels) {
        const auto level = spdlog::level::to_string_view(message.level);

        fmt::format_to(
            std::back_inserter(buffer),
            "[{}] ",
            level);
    }

    fmt::format_to(
        std::back_inserter(buffer),
        "{}",
        message.text);

    return fmt::to_string(buffer);
}
