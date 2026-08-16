#include "utils/imgui_utils.hpp"

namespace ImGuiUtils
{
namespace
{

float labelWidth(char const* label)
{
    return ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label, nullptr, true).x;
}

} // namespace

FlowLayout::FlowLayout(float itemWidth) noexcept
    : itemWidth_{itemWidth},
      rightEdge_{ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x}
{}

bool FlowLayout::button(char const* label)
{
    sameLineIfFits(ImGui::CalcTextSize(label, nullptr, true).x + (ImGui::GetStyle().FramePadding.x * 2.0f));
    return ImGui::Button(label);
}

void FlowLayout::toggle(Knob<bool>& knob)
{
    sameLineIfFits(ImGui::GetFrameHeight() + labelWidth(knob.name()));
    knob.render();
}

void FlowLayout::field(KnobBase& knob)
{
    sameLineIfFits(itemWidth_ + labelWidth(knob.name()));
    ImGui::SetNextItemWidth(itemWidth_);
    knob.render();
}

void FlowLayout::sameLineIfFits(float nextWidth)
{
    if (!started_) {
        started_ = true;
        return;
    }

    if ((ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + nextWidth) <= rightEdge_) {
        ImGui::SameLine();
    }
}

} // namespace ImGuiUtils
