#ifndef YARR_UTILS_META_HPP
#define YARR_UTILS_META_HPP

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

template <std::size_t... Idxs>
constexpr auto substringAsArray(std::string_view str, std::index_sequence<Idxs...>)
{
    return std::array {str[Idxs] ...};
}

template <typename T>
constexpr auto typeNameArray()
{
#if defined(__clang__)
    constexpr auto PREFIX   = std::string_view {"[T = "};
    constexpr auto SUFFIX   = std::string_view {"]"};
    constexpr auto FUNCTION = std::string_view {__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
    constexpr auto PREFIX   = std::string_view {"with T = "};
    constexpr auto SUFFIX   = std::string_view {"]"};
    constexpr auto FUNCTION = std::string_view {__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
    constexpr auto PREFIX   = std::string_view {"typeNameArray<"};
    constexpr auto SUFFIX   = std::string_view {">(void)"};
    constexpr auto FUNCTION = std::string_view {__FUNCSIG__};
#else
# error Unsupported compiler
#endif

    constexpr auto START = FUNCTION.find(PREFIX) + PREFIX.size();
    constexpr auto END   = FUNCTION.rfind(SUFFIX);

    static_assert(START < END);

    constexpr auto NAME = FUNCTION.substr(START, (END - START));
    return substringAsArray(NAME, std::make_index_sequence<NAME.size()>{});
}

template <typename T>
struct TypeNameHolder
{
    static inline constexpr auto VALUE = typeNameArray<T>();
};

template <typename T>
constexpr auto typeName() -> std::string_view
{
    constexpr auto& VALUE = TypeNameHolder<T>::VALUE;
    return std::string_view {VALUE.data(), VALUE.size()};
}

#endif // YARR_UTILS_META_HPP
