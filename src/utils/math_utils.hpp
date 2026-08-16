#ifndef YARR_UTILS_MATH_UTILS_HPP
#define YARR_UTILS_MATH_UTILS_HPP

[[nodiscard]] constexpr float intPow(float base, int exponent) noexcept
{
    float     result    = 1.0F;
    int const magnitude = exponent < 0? -exponent : exponent;

    for (int i = 0; i < magnitude; ++i) {
        result *= base;
    }

    return (exponent < 0)? 1.0F / result : result;
}

#endif // YARR_UTILS_MATH_UTILS_HPP
