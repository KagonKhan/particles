#include "app/console.hpp"

#include "app/styling.hpp"
#include "utils/imgui_utils.hpp"
#include "utils/time_utils.hpp"

#include <fmt/format.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <iterator>
#include <utility>

using imgui_utils::FlowLayout;
using styling::messageColor;

namespace
{

constexpr char const* const OPTIONS_POPUP {"console options"};

} // namespace


void OutputConsole::update()
{
    applySettings();
    log_.drain(*sink_);
}

void OutputConsole::render()
{
    if (!panelVisible_.get()) {
        return;
    }

    if (ImGui::Begin("Console Log", panelVisible_.address())) {
        renderToolbar();

        applySettings();

        renderMessages();
    }

    ImGui::End();
}

void OutputConsole::applySettings()
{
    log_.setCapacity(static_cast<std::size_t>(maxMessages_.get()));
    log_.setLevel(static_cast<spdlog::level::level_enum>(levelFilter_.index()));
}

void OutputConsole::renderToolbar()
{
    FlowLayout layout {100.0F};

    if (layout.button("clear")) {
        log_.clear();
    }

    layout.field(levelFilter_);

    if (layout.button("options")) {
        ImGui::OpenPopup(OPTIONS_POPUP);
    }

    renderOptions();
}

void OutputConsole::renderOptions()
{
    if (!ImGui::BeginPopup(OPTIONS_POPUP)) {
        return;
    }

    showTimestamps_.render();
    showLevels_.render();
    wrapText_.render();
    autoScroll_.render();

    ImGui::SetNextItemWidth(100.0F);
    maxMessages_.render();

    ImGui::EndPopup();
}

void OutputConsole::renderMessages()
{
    ImGui::BeginChild(
        "messages",
        ImVec2 {0, 0},
        ImGuiChildFlags_Borders,
        ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 {0, 1});

        if (wrapText_.get()) {
            renderWrapped();
        }
        else {
            renderClipped();
        }

        ImGui::PopStyleVar();

        if (autoScroll_.get() && (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void OutputConsole::renderClipped()
{
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(log_.rowCount()));

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            drawRow(static_cast<std::size_t>(row));
        }
    }

    clipper.End();
}

void OutputConsole::renderWrapped()
{
    std::size_t const count = log_.rowCount();

    if (count == 0) {
        return;
    }

    float const width   = ImGui::GetContentRegionAvail().x;
    float const total   = syncRowOffsets(width);
    float const base    = wrapped_.tops.front();
    float const start_y = ImGui::GetCursorPosY();

    auto const above = std::upper_bound(
        wrapped_.tops.begin(),
        wrapped_.tops.end(),
        base + ImGui::GetScrollY());
    auto const below = std::lower_bound(
        above,
        wrapped_.tops.end(),
        base + ImGui::GetScrollY() + ImGui::GetWindowHeight());

    auto const first = static_cast<std::size_t>(std::distance(wrapped_.tops.begin(), above)) - 1;
    auto const last  = std::min(static_cast<std::size_t>(std::distance(wrapped_.tops.begin(), below)), count);

    ImGui::PushTextWrapPos(0.0f);
    ImGui::SetCursorPosY(start_y + (wrapped_.tops[first] - base));

    for (std::size_t row = first; row < last; ++row) {
        drawRow(row);
    }

    ImGui::PopTextWrapPos();

    // The skipped tail still has to be accounted for, or the scroll range collapses.
    ImGui::SetCursorPosY(start_y + total);
    ImGui::Dummy(ImVec2 {0, 0});
}

float OutputConsole::syncRowOffsets(float width)
{
    std::size_t const count      = log_.rowCount();
    int const         decoration = (showTimestamps_.get()? 2 : 0) | (showLevels_.get()? 1 : 0);

    // Rows only ever leave from the front, so dropping as many cached offsets keeps the rest
    // pointing at the rows they were measured from. Everything else invalidates the lot.
    std::uint64_t const dropped = (log_.firstRow() > wrapped_.firstRow)? (log_.firstRow() - wrapped_.firstRow) : 0;

    bool const stale = (wrapped_.generation != log_.generation())
        || (wrapped_.width != width)
        || (wrapped_.decoration != decoration)
        || (dropped >= wrapped_.tops.size());

    if (stale) {
        wrapped_.tops.assign(1, 0.0f);
    }
    else {
        wrapped_.tops.erase(wrapped_.tops.begin(), wrapped_.tops.begin() + static_cast<std::ptrdiff_t>(dropped));
    }

    wrapped_.width      = width;
    wrapped_.decoration = decoration;
    wrapped_.firstRow   = log_.firstRow();
    wrapped_.generation = log_.generation();

    float const spacing = ImGui::GetStyle().ItemSpacing.y;

    for (std::size_t row = wrapped_.tops.size() - 1; row < count; ++row) {
        std::string_view const line   = formatMessage(log_.row(row));
        float const            height = ImGui::CalcTextSize(line.data(), line.data() + line.size(), false, width).y;

        wrapped_.tops.push_back(wrapped_.tops.back() + height + spacing);
    }

    return wrapped_.tops.back() - wrapped_.tops.front();
}

void OutputConsole::drawRow(std::size_t row)
{
    ConsoleMessage const&  message = log_.row(row);
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

    scratch_.clear();

    if (showTimestamps_.get()) {
        time_utils::appendLocalTime(scratch_, message.timestamp);
        scratch_.push_back(' ');
    }

    if (showLevels_.get()) {
        fmt::format_to(std::back_inserter(scratch_), "[{}] ", spdlog::level::to_string_view(message.level));
    }

    scratch_.append(message.text);
    return scratch_;
}
