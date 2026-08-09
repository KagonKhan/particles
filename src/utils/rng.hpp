#ifndef YARR_RNG_HPP
#define YARR_RNG_HPP

#include <algorithm>
#include <cmath>
#include <random>

class RNG
{
public:
    RNG()
        : engine_(std::random_device{}()) {}
    explicit RNG(std::uint32_t seed)
        : engine_(seed) {}

    // Uniform float in [min, max)
    float range(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(engine_);
    }

    // Uniform int in [min, max] (inclusive)
    int range(int min, int max)
    {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine_);
    }

    // Uniform angle in radians [0, 2*pi)
    float angle()
    {
        return range(0.0f, 6.28318530718f);
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
        z = range(-1.0f, 1.0f);

        float yaw    = angle();
        float radius = std::sqrt(std::max(0.0f, 1.0f - (z * z)));

        x = radius * std::cos(yaw);
        y = radius * std::sin(yaw);
    }

    // 0.0 to 1.0
    float normalized()
    {
        return range(0.0f, 1.0f);
    }

    // Coin flip
    bool chance(float probability = 0.5f)
    {
        return normalized() < probability;
    }

private:
    std::mt19937 engine_;
};

#endif // YARR_RNG_HPP
