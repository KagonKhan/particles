#ifndef YARR_UTILS_RNG_HPP
#define YARR_UTILS_RNG_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

class Rng
{
public:
    Rng()
        : engine_(std::random_device{}()) {}
    explicit Rng(std::uint32_t seed)
        : engine_(seed) {}

    // Uniform float in [min, max)
    [[nodiscard]] float range(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(engine_);
    }

    // Uniform int in [min, max] (inclusive)
    [[nodiscard]] int range(int min, int max)
    {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine_);
    }

    // Uniform angle in radians [0, 2*pi)
    [[nodiscard]] float angle()
    {
        return range(0.0F, 2.0F * std::numbers::pi_v<float>);
    }

    // Uniform point on the unit circle, useful for random directions
    void unitVector(float& x, float& y)
    {
        float a = angle();
        x = std::cos(a);
        y = std::sin(a);
    }

    // Uniform point on the unit sphere. Sampling z directly rather than a pitch
    // angle is what keeps the distribution even — picking pitch uniformly would
    // bunch the directions up around the poles.
    void unitVector(float& x, float& y, float& z)
    {
        z = range(-1.0F, 1.0F);

        float yaw    = angle();
        float radius = std::sqrt(std::max(0.0F, 1.0F - (z * z)));

        x = radius * std::cos(yaw);
        y = radius * std::sin(yaw);
    }

    // 0.0 to 1.0
    [[nodiscard]] float normalized()
    {
        return range(0.0F, 1.0F);
    }

    // Coin flip
    [[nodiscard]] bool chance(float probability = 0.5F)
    {
        return normalized() < probability;
    }

private:
    std::mt19937 engine_;
};

#endif // YARR_UTILS_RNG_HPP
