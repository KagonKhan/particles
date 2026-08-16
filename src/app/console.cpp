#include "app/console.hpp"

#include "app/styling.hpp"
#include "utils/imgui_utils.hpp"
#include "utils/time_utils.hpp"

#include <fmt/format.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iterator>

using imgui_utils::FlowLayout;
using styling::messageColor;

namespace
{

constexpr char const* OPTIONS_POPUP {"console options"};
constexpr float       ITEM_WIDTH    = {100.0F};
constexpr float       SCROLL_SLACK  = {1.0F};
constexpr float       GRAB_MIN_SIZE = {128.0F};
constexpr float       ROW_SPACING   = {1.0F};

} // namespace


// Capacity and level are applied here rather than while drawing: the panel can be hidden for the
// whole run, and an unapplied capacity leaves the history unbounded.
void OutputConsole::update()
{
    applySettings();
    logBuffer_.drain(*sink_);
}

void OutputConsole::render()
{
    if (!panelVisible_.get()) {
        return;
    }

    if (ImGui::Begin("Console Log", panelVisible_.address())) {
        renderToolbar();
        renderMessages();
    }

    ImGui::End();
}

void OutputConsole::applySettings()
{
    logBuffer_.setCapacity(static_cast<std::size_t>(maxMessages_.get()));
    logBuffer_.setLevel(static_cast<spdlog::level::level_enum>(levelFilter_.index()));
}

void OutputConsole::renderToolbar()
{
    FlowLayout layout {ITEM_WIDTH};

    if (layout.button("clear")) {
        logBuffer_.clear();
    }

    layout.field(levelFilter_);

    if (layout.button("options")) {
        ImGui::OpenPopup(OPTIONS_POPUP);
    }

    if (ImGui::BeginPopup(OPTIONS_POPUP)) {
        showTimestamps_.render();
        showLevels_.render();
        wrapText_.render();

        ImGui::SetNextItemWidth(ITEM_WIDTH);
        maxMessages_.render();

        ImGui::EndPopup();
    }
}

void OutputConsole::renderMessages()
{
    std::size_t const count = logBuffer_.rowCount();

    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, GRAB_MIN_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 {0, ROW_SPACING});

    float const content_height = measureContent(count);
    ImGui::SetNextWindowContentSize(ImVec2 {0.0F, content_height});

    bool const wheeled = (ImGui::GetIO().MouseWheel != 0.0F) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    if (stickToBottom_ && !wheeled) {
        ImGui::SetNextWindowScroll(ImVec2 {-1.0F, content_height});
    }

    ImGui::BeginChild("messages", ImVec2 {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    contentWidth_ = ImGui::GetContentRegionAvail().x;

    if (count > 0) {
        if (wrapText_.get()) {
            float const base        = wrapped_.tops.front();
            float const start_y     = ImGui::GetCursorPosY();
            float const view_top    = base + ImGui::GetScrollY();
            float const view_bottom = view_top + ImGui::GetWindowHeight();

            auto const above = std::ranges::upper_bound(wrapped_.tops, view_top);
            auto const below = std::lower_bound(above, wrapped_.tops.end(), view_bottom);

            auto const first = static_cast<std::size_t>(std::distance(wrapped_.tops.begin(), above)) - 1;
            auto const last  = std::min(static_cast<std::size_t>(std::distance(wrapped_.tops.begin(), below)), count);

            ImGui::PushTextWrapPos(0.0F);
            ImGui::SetCursorPosY(start_y + (wrapped_.tops[first] - base));

            for (std::size_t row = first; row < last; ++row) {
                drawRow(row);
            }

            ImGui::PopTextWrapPos();
            ImGui::SetCursorPosY(start_y + content_height);
            ImGui::Dummy(ImVec2 {0, 0});
        }
        else {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(count));

            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    drawRow(static_cast<std::size_t>(row));
                }
            }

            clipper.End();
        }
    }

    stickToBottom_ = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - SCROLL_SLACK;

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

float OutputConsole::syncRowOffsets(float width)
{
    std::size_t const count      = logBuffer_.rowCount();
    int const         decoration = (showTimestamps_.get()? 2 : 0) | (showLevels_.get()? 1 : 0);

    // Rows only ever leave from the front, so dropping as many cached offsets keeps the rest
    // pointing at the rows they were measured from. Everything else invalidates the lot.
    std::uint64_t const dropped = (logBuffer_.firstRow() >
        wrapped_.firstRow)? (logBuffer_.firstRow() - wrapped_.firstRow) : 0;

    bool const stale = (wrapped_.generation != logBuffer_.generation())
        || (wrapped_.width != width)
        || (wrapped_.decoration != decoration)
        || (dropped >= wrapped_.tops.size());

    if (stale) {
        wrapped_.tops.assign(1, 0.0F);
    }
    else {
        wrapped_.tops.erase(wrapped_.tops.begin(), wrapped_.tops.begin() + static_cast<std::ptrdiff_t>(dropped));
    }

    wrapped_.width      = width;
    wrapped_.decoration = decoration;
    wrapped_.firstRow   = logBuffer_.firstRow();
    wrapped_.generation = logBuffer_.generation();

    float const spacing = ImGui::GetStyle().ItemSpacing.y;

    for (std::size_t row = wrapped_.tops.size() - 1; row < count; ++row) {
        std::string_view const line   = formatMessage(logBuffer_.row(row));
        float const            height = ImGui::CalcTextSize(line.data(), line.data() + line.size(), false, width).y;

        wrapped_.tops.push_back(wrapped_.tops.back() + height + spacing);
    }

    return wrapped_.tops.back() - wrapped_.tops.front();
}

float OutputConsole::measureContent(std::size_t count)
{
    if (count == 0) {
        return 0.0F;
    }

    if (!wrapText_.get()) {
        return (static_cast<float>(count) * (ImGui::GetTextLineHeight() + ROW_SPACING)) - ROW_SPACING;
    }

    // Wrapping is measured against the width the child reported last frame, since this runs before
    // the child exists. A resize costs one frame of misplaced rows.
    return (contentWidth_ > 0.0F)? syncRowOffsets(contentWidth_) : 0.0F;
}

void OutputConsole::drawRow(std::size_t row)
{
    ConsoleMessage const&  message = logBuffer_.row(row);
    std::string_view const line    = formatMessage(message);

    ImGui::PushStyleColor(ImGuiCol_Text, messageColor(message.level));
    ImGui::TextUnformatted(line.data(), line.data() + line.size());
    ImGui::PopStyleColor();
}

std::string_view OutputConsole::formatMessage(ConsoleMessage const& message)
{
    if (!showTimestamps_.get() && !showLevels_.get()) {
        return message.text;
    }

    scratchPad_.clear();

    if (showTimestamps_.get()) {
        time_utils::appendLocalTime(scratchPad_, message.timestamp);
        scratchPad_.push_back(' ');
    }

    if (showLevels_.get()) {
        fmt::format_to(std::back_inserter(scratchPad_), "[{}] ", spdlog::level::to_string_view(message.level));
    }

    scratchPad_.append(message.text);
    return scratchPad_;
}
