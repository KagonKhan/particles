#ifndef YARR_UTILS_IMGUI_UTILS_HPP
#define YARR_UTILS_IMGUI_UTILS_HPP


#include "utils/knob.hpp"

#include <imgui.h>

namespace imgui_utils
{

/// @brief layout manager that auto splits to next line if no space
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
