#ifndef YARR_RNG_HPP
#define YARR_RNG_HPP

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

    void unitVector(float& x, float& y, float& z)
    {
        float yaw   = angle(); // Angle around the Z-axis (in radians)
        float pitch = angle(); // Angle above/below the XY-plane (in radians)

        x = std::cos(pitch) * std::cos(yaw);
        y = std::cos(pitch) * std::sin(yaw);
        z = std::sin(pitch);
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
