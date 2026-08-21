#include "utils/time_utils.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <exception>
#include <iterator>

namespace time_utils
{

namespace
{

std::chrono::time_zone const* localZone()
{
    static std::chrono::time_zone const* const zone = [] () -> std::chrono::time_zone const* {
            try {
                return std::chrono::current_zone();
            }
            catch (std::exception const&) {
                return nullptr;
            }
        } ();

    return zone;
}

} // namespace

void appendLocalTime(std::string& out, std::chrono::system_clock::time_point when)
{
    auto const timestamp = std::chrono::floor<std::chrono::milliseconds>(when);
    auto       sink      = std::back_inserter(out);

    if (std::chrono::time_zone const* const zone = localZone()) {
        fmt::format_to(sink, "{:%H:%M:%S}", zone->to_local(timestamp));
    }
    else {
        fmt::format_to(sink, "{:%H:%M:%S}", timestamp);
    }
}

} // namespace time_utils
