#ifndef YARR_UTILS_TIME_UTILS_HPP
#define YARR_UTILS_TIME_UTILS_HPP


#include <chrono>
#include <string>

namespace time_utils
{

void appendLocalTime(std::string& out, std::chrono::system_clock::time_point when);

} // namespace time_utils

#endif // YARR_UTILS_TIME_UTILS_HPP
