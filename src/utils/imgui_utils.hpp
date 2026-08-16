#ifndef YARR_UTILS_IMGUI_UTILS_HPP
#define YARR_UTILS_IMGUI_UTILS_HPP


#include "utils/knob.hpp"

#include <imgui.h>

namespace imgui_utils
{

// A toolbar whose items stay on one line while they fit and wrap onto the next when they do
// not. ImGui only knows an item's size once it has been submitted, so each item's width has to
// be predicted before deciding to keep it on the current line.
//
// Construct it where the toolbar begins: that cursor position is what fixes the right edge.
class FlowLayout
{
public:
    explicit FlowLayout(float item_width) noexcept;

    [[nodiscard]] bool button(char const* label);

    void toggle(Knob<bool>& knob);
    void field(KnobBase& knob);

private:
    void sameLineIfFits(float next_width);

    float itemWidth_;
    float rightEdge_;
    bool  started_ {false};
};

} // namespace imgui_utils

#endif // YARR_UTILS_IMGUI_UTILS_HPP
