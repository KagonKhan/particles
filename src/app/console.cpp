#include "app/console.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>


namespace
{

ImU32 messageColor(spdlog::level::level_enum level)
{
    // Indexed by spdlog::level::level_enum: trace, debug, info, warn, err, critical, off.
    static constexpr std::array<ImU32, spdlog::level::n_levels> LEVEL_COLORS {
        IM_COL32(110, 110, 110, 255), // trace
        IM_COL32(140, 140, 140, 255), // debug
        IM_COL32(112, 179, 123, 255), // info
        IM_COL32(193, 102, 1, 255),   // warn
        IM_COL32(198, 0, 3, 255),     // err
        IM_COL32(198, 0, 129, 255),   // critical
        IM_COL32(255, 255, 255, 255), // off
    };

    auto const index = static_cast<std::size_t>(level);
    return LEVEL_COLORS[std::min(index, LEVEL_COLORS.size() - 1)];
}

// ImGui only knows an item's size once it has been submitted, so wrapping a toolbar means
// predicting the width of the next item before deciding to keep it on the current line.
float checkboxWidth(char const* label)
{
    return ImGui::GetFrameHeight() // the square
           + ImGui::GetStyle().ItemInnerSpacing.x
           + ImGui::CalcTextSize(label, nullptr, true).x;
}

// An item submitted after SetNextItemWidth(), plus its trailing label.
float labeledItemWidth(float itemWidth, char const* label)
{
    return itemWidth
           + ImGui::GetStyle().ItemInnerSpacing.x
           + ImGui::CalcTextSize(label, nullptr, true).x;
}

} // namespace

OutputConsole::OutputConsole()
{
    spdlog::default_logger()->sinks().push_back(sink);
    messages.reserve(static_cast<std::size_t>(settings.maxMessages));
    visible.reserve(static_cast<std::size_t>(settings.maxMessages));
}

void OutputConsole::render(ImVec2 size)
{
    ImGui::SeparatorText("Console");

    renderToolbar();

    pullNewMessages();
    rebuildFilterIfNeeded();

    renderMessages(size);
}

void OutputConsole::renderToolbar()
{
    // Toolbar items stay on one line while they fit and wrap onto the next when they do not.
    static constexpr float FIELD_WIDTH {100.0f};

    float const rightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

    auto const sameLineIfFits = [rightEdge] (float nextWidth) {
            float const nextX =
                ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + nextWidth;

            if (nextX < rightEdge) {
                ImGui::SameLine();
            }
        };

    if (ImGui::Button("clear")) {
        messages.clear();
        visible.clear();
        filteredCount = 0;
    }

    sameLineIfFits(labeledItemWidth(FIELD_WIDTH, "Filter messages"));
    ImGui::SetNextItemWidth(FIELD_WIDTH);
    ImGui::Combo(
        "Filter messages",
        &settings.selectedMessageLevel,
        "trace\0debug\0info\0warning\0error\0critical\0\0"
    );

    sameLineIfFits(checkboxWidth("Timestamps"));
    ImGui::Checkbox("Timestamps", &settings.showTimestamps);

    sameLineIfFits(checkboxWidth("Levels"));
    ImGui::Checkbox("Levels", &settings.showLevels);

    sameLineIfFits(checkboxWidth("Wrap"));
    ImGui::Checkbox("Wrap", &settings.wrapText);

    sameLineIfFits(checkboxWidth("Auto-scroll"));
    ImGui::Checkbox("Auto-scroll", &settings.autoScroll);

    sameLineIfFits(labeledItemWidth(FIELD_WIDTH, "Max messages"));
    ImGui::SetNextItemWidth(FIELD_WIDTH);
    ImGui::DragInt(
        "Max messages",
        &settings.maxMessages,
        8.0f,
        MIN_MAX_MESSAGES,
        MAX_MAX_MESSAGES,
        "%d",
        ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Oldest messages are dropped once the history exceeds this.");
}

void OutputConsole::renderMessages(ImVec2 size)
{
    ImGui::BeginChild(TAG, size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 {0, 1});

        if (settings.wrapText) {
            // Wrapped rows have no fixed height, so every one of them has to be submitted.
            ImGui::PushTextWrapPos(0.0f);
            for (int index : visible) {
                drawMessage(index);
            }

            ImGui::PopTextWrapPos();
        }
        else {
            // Uniform line height: only the rows actually on screen get formatted and drawn.
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(visible.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    drawMessage(visible[static_cast<std::size_t>(row)]);
                }
            }

            clipper.End();
        }

        ImGui::PopStyleVar();

        if (settings.autoScroll && (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void OutputConsole::drawMessage(int index)
{
    ConsoleMessage const& message = messages[static_cast<std::size_t>(index)];
    std::string_view      line    = formatMessage(message);

    ImGui::PushStyleColor(ImGuiCol_Text, messageColor(message.level));
    ImGui::TextUnformatted(line.data(), line.data() + line.size());
    ImGui::PopStyleColor();
}

void OutputConsole::pullNewMessages()
{
    sink->drain(messages);

    auto const cap = static_cast<std::size_t>(std::clamp(settings.maxMessages, MIN_MAX_MESSAGES, MAX_MAX_MESSAGES));

    if (messages.size() <= cap) {
        return;
    }

    // Drop the oldest entries and shift the cached indices to match.
    std::size_t const dropped = messages.size() - cap;
    messages.erase(messages.begin(), messages.begin() + static_cast<std::ptrdiff_t>(dropped));

    auto const shift = static_cast<int>(dropped);
    auto const first = std::find_if(
        visible.begin(),
        visible.end(),
        [shift] (int index) { return index >= shift; });

    visible.erase(visible.begin(), first);
    for (int& index : visible) {
        index -= shift;
    }

    filteredCount -= std::min(filteredCount, dropped);
}

void OutputConsole::rebuildFilterIfNeeded()
{
    if (filteredLevel != settings.selectedMessageLevel) {
        filteredLevel = settings.selectedMessageLevel;
        filteredCount = 0;
        visible.clear();
    }

    // Only the messages appended since the last frame need to be classified.
    for (std::size_t index = filteredCount; index < messages.size(); ++index) {
        if (messages[index].level >= settings.selectedMessageLevel) {
            visible.push_back(static_cast<int>(index));
        }
    }

    filteredCount = messages.size();
}

std::string_view OutputConsole::formatMessage(ConsoleMessage const& message)
{
    if (!settings.showTimestamps && !settings.showLevels) {
        return message.text;
    }

    scratch.clear();
    auto out = std::back_inserter(scratch);

    if (settings.showTimestamps) {
        auto const time = std::chrono::system_clock::to_time_t(message.timestamp);

        std::tm local_time {};
#ifdef _WIN32
        localtime_s(&local_time, &time);
#else
        localtime_r(&time, &local_time);
#endif

        auto const milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()) %
            std::chrono::seconds {1};

        fmt::format_to(out, "{:%H:%M:%S}.{:03} ", local_time, milliseconds.count());
    }

    if (settings.showLevels) {
        fmt::format_to(out, "[{}] ", spdlog::level::to_string_view(message.level));
    }

    scratch.append(message.text);
    return scratch;
}
