#include "app/styling.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Styling
{

ImU32 messageColor(spdlog::level::level_enum level)
{
    // Indexed by spdlog::level::level_enum: trace, debug, info, warn, err, critical, off.
    static constexpr std::array<ImU32, spdlog::level::n_levels> LEVEL_COLORS {
        IM_COL32(110, 110, 110, 255), // trace
        IM_COL32(140, 140, 140, 255), // debug
        IM_COL32(112, 179, 123, 255), // info
        IM_COL32(193, 102, 1, 255),   // warn
        IM_COL32(198, 0, 3, 255),     // err
        IM_COL32(198, 0, 129, 255),   // critical
        IM_COL32(255, 255, 255, 255), // off
    };

    auto const index = static_cast<std::size_t>(level);
    return LEVEL_COLORS[std::min(index, LEVEL_COLORS.size() - 1)];
}

} // namespace Styling
