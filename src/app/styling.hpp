#ifndef YARR_APP_STYLING_HPP
#define YARR_APP_STYLING_HPP


#include <imgui.h>
#include <spdlog/common.h>

namespace styling
{

[[nodiscard]] ImU32 messageColor(spdlog::level::level_enum level);

} // namespace styling

#endif // YARR_APP_STYLING_HPP
