#include "app/settings.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string_view>


namespace
{

// Widths in the menu are given in font sizes, so the bar keeps its proportions at any font scale.
constexpr float ITEM_WIDTH_IN_FONT_SIZES = 9.0F;

} // namespace

void Settings::render()
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    for (Menu& menu : menus_) {
        if (!ImGui::BeginMenu(menu.name)) {
            continue;
        }

        for (std::unique_ptr<KnobBase> const& knob : menu.knobs) {
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * ITEM_WIDTH_IN_FONT_SIZES);
            knob->render();
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

KnobBase* Settings::findKnob(char const* menu, char const* name)
{
    for (Menu& candidate : menus_) {
        if (std::string_view {candidate.name} != menu) {
            continue;
        }

        for (std::unique_ptr<KnobBase> const& knob : candidate.knobs) {
            if (std::string_view {knob->name()} == name) {
                return knob.get();
            }
        }
    }

    return nullptr;
}
