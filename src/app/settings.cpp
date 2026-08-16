#include "app/settings.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <string_view>


namespace
{

// In font sizes. A menu fits itself to what is in it, and a slider left to its own devices
// asks for the width of the window it is in, which between them size nothing sensibly.
constexpr float kItemWidth = 9.0F;

} // namespace


void Settings::describe(char const* menu, char const* name, char const* tooltip)
{
    Entry* const entry = findEntry(menu, name);

    if (entry == nullptr) {
        spdlog::warn("No setting \"{}/{}\" to describe", menu, name);
        return;
    }

    entry->tooltip = tooltip;
}

void Settings::render()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    for (Menu& menu : menus_) {
        if (!ImGui::BeginMenu(menu.name)) {
            continue;
        }

        for (Entry& entry : menu.entries) {
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * kItemWidth);
            entry.knob->render();

            if (entry.tooltip != nullptr) {
                ImGui::SetItemTooltip("%s", entry.tooltip);
            }
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

Settings::Entry* Settings::findEntry(char const* menu, char const* name)
{
    for (Menu& candidate : menus_) {
        if (std::string_view {candidate.name} != menu) {
            continue;
        }

        for (Entry& entry : candidate.entries) {
            if (std::string_view {entry.knob->name()} == name) {
                return &entry;
            }
        }
    }

    return nullptr;
}

std::vector<Settings::Entry>& Settings::entriesOf(char const* menu)
{
    for (Menu& candidate : menus_) {
        if (std::string_view {candidate.name} == menu) {
            return candidate.entries;
        }
    }

    menus_.push_back(Menu {menu, {}});

    return menus_.back().entries;
}
