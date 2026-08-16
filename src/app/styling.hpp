#ifndef YARR_APP_STYLING_HPP
#define YARR_APP_STYLING_HPP


#include <imgui.h>
#include <spdlog/common.h>

namespace Styling
{

[[nodiscard]] ImU32 messageColor(spdlog::level::level_enum level);

} // namespace Styling

#endif // YARR_APP_STYLING_HPP
