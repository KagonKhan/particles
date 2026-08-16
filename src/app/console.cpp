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

using ImGuiUtils::FlowLayout;
using Styling::messageColor;

OutputConsole::OutputConsole(std::shared_ptr<ImGuiConsoleSink> logSink)
    : sink{std::move(logSink)}
{
    Settings::getInstance().describe("View", "Console", "The log panel. Messages keep arriving while it is hidden.");
}

void OutputConsole::update()
{
    applySettings();
    log.drain(*sink);
}

void OutputConsole::render()
{
    if (!panelVisible.get()) {
        return;
    }

    if (ImGui::Begin("Console Log", panelVisible.address())) {
        renderToolbar();

        // Again, so a change just made in the toolbar takes effect in this frame rather than
        // the next one. Both setters are no-ops when the value has not moved.
        applySettings();

        renderMessages();
    }

    ImGui::End();
}

void OutputConsole::applySettings()
{
    log.setCapacity(static_cast<std::size_t>(maxMessages.get()));
    log.setLevel(static_cast<spdlog::level::level_enum>(levelFilter.index()));
}

void OutputConsole::renderToolbar()
{
    FlowLayout layout {FIELD_WIDTH};

    if (layout.button("clear")) {
        log.clear();
    }

    layout.field(levelFilter);

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

    showTimestamps.render();
    showLevels.render();
    wrapText.render();
    autoScroll.render();

    ImGui::SetNextItemWidth(FIELD_WIDTH);
    maxMessages.render();

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

        if (wrapText.get()) {
            renderWrapped();
        }
        else {
            renderClipped();
        }

        ImGui::PopStyleVar();

        if (autoScroll.get() && (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void OutputConsole::renderClipped()
{
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(log.rowCount()));

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            drawRow(static_cast<std::size_t>(row));
        }
    }

    clipper.End();
}

void OutputConsole::renderWrapped()
{
    std::size_t const count = log.rowCount();

    if (count == 0) {
        return;
    }

    float const width  = ImGui::GetContentRegionAvail().x;
    float const total  = syncRowOffsets(width);
    float const base   = wrapped.tops.front();
    float const startY = ImGui::GetCursorPosY();

    auto const above = std::upper_bound(
        wrapped.tops.begin(),
        wrapped.tops.end(),
        base + ImGui::GetScrollY());
    auto const below = std::lower_bound(
        above,
        wrapped.tops.end(),
        base + ImGui::GetScrollY() + ImGui::GetWindowHeight());

    auto const first = static_cast<std::size_t>(std::distance(wrapped.tops.begin(), above)) - 1;
    auto const last  = std::min(static_cast<std::size_t>(std::distance(wrapped.tops.begin(), below)), count);

    ImGui::PushTextWrapPos(0.0f);
    ImGui::SetCursorPosY(startY + (wrapped.tops[first] - base));

    for (std::size_t row = first; row < last; ++row) {
        drawRow(row);
    }

    ImGui::PopTextWrapPos();

    // The skipped tail still has to be accounted for, or the scroll range collapses.
    ImGui::SetCursorPosY(startY + total);
    ImGui::Dummy(ImVec2 {0, 0});
}

float OutputConsole::syncRowOffsets(float width)
{
    std::size_t const count      = log.rowCount();
    int const         decoration = (showTimestamps.get()? 2 : 0) | (showLevels.get()? 1 : 0);

    // Rows only ever leave from the front, so dropping as many cached offsets keeps the rest
    // pointing at the rows they were measured from. Everything else invalidates the lot.
    std::uint64_t const dropped = (log.firstRow() > wrapped.firstRow)? (log.firstRow() - wrapped.firstRow) : 0;

    bool const stale = (wrapped.generation != log.generation())
                    || (wrapped.width != width)
                    || (wrapped.decoration != decoration)
                    || (dropped >= wrapped.tops.size());

    if (stale) {
        wrapped.tops.assign(1, 0.0f);
    }
    else {
        wrapped.tops.erase(wrapped.tops.begin(), wrapped.tops.begin() + static_cast<std::ptrdiff_t>(dropped));
    }

    wrapped.width      = width;
    wrapped.decoration = decoration;
    wrapped.firstRow   = log.firstRow();
    wrapped.generation = log.generation();

    float const spacing = ImGui::GetStyle().ItemSpacing.y;

    for (std::size_t row = wrapped.tops.size() - 1; row < count; ++row) {
        std::string_view const line = formatMessage(log.row(row));
        float const height = ImGui::CalcTextSize(line.data(), line.data() + line.size(), false, width).y;

        wrapped.tops.push_back(wrapped.tops.back() + height + spacing);
    }

    return wrapped.tops.back() - wrapped.tops.front();
}

void OutputConsole::drawRow(std::size_t row)
{
    ConsoleMessage const&  message = log.row(row);
    std::string_view const line    = formatMessage(message);

    ImGui::PushStyleColor(ImGuiCol_Text, messageColor(message.level));
    ImGui::TextUnformatted(line.data(), line.data() + line.size());
    ImGui::PopStyleColor();
}

std::string_view OutputConsole::formatMessage(ConsoleMessage const& message)
{
    if (!showTimestamps.get() && !showLevels.get()) {
        return message.text;
    }

    scratch.clear();

    if (showTimestamps.get()) {
        TimeUtils::appendLocalTime(scratch, message.timestamp);
        scratch.push_back(' ');
    }

    if (showLevels.get()) {
        fmt::format_to(std::back_inserter(scratch), "[{}] ", spdlog::level::to_string_view(message.level));
    }

    scratch.append(message.text);
    return scratch;
}
