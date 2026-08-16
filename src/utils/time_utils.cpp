#include "utils/time_utils.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <exception>
#include <iterator>

namespace TimeUtils
{
namespace
{

// Resolved once. The lookup reads the system's time zone database and throws when there is
// none, which is not something a log line should have to survive: no zone means UTC.
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
    // Truncating to milliseconds first is what puts ".mmm" in the output: %S prints a
    // time_point's own precision, and system_clock's is far finer than anyone can read.
    auto const timestamp = std::chrono::floor<std::chrono::milliseconds>(when);
    auto       sink      = std::back_inserter(out);

    if (std::chrono::time_zone const* const zone = localZone()) {
        fmt::format_to(sink, "{:%H:%M:%S}", zone->to_local(timestamp));
    }
    else {
        fmt::format_to(sink, "{:%H:%M:%S}", timestamp);
    }
}

} // namespace TimeUtils
