#ifndef YARR_EMITTER_HPP
#define YARR_EMITTER_HPP

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>


static constexpr std::size_t MAX_PARTICLES = 1'000'000;
struct Particle
{
    float x, y;
    float velocityX, velocityY;
    float age; // seconds remaining
};


struct ParticlePool
{
    std::array<ImVec2, MAX_PARTICLES> positions {};
    std::array<ImVec2, MAX_PARTICLES> velocities {};
    std::array<float, MAX_PARTICLES> ages {};
};


#include <cstdint>
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

class Emitter
{
public:
    static constexpr std::size_t MAX_PARTICLES = 1'000'000;


    [[nodiscard]] std::size_t   aliveCount() const noexcept { return aliveCount_; }
    [[nodiscard]] ImVec2 const* data() const noexcept       { return pool_.positions.data(); }

    void spawn(ImVec2 position, float dt)
    {
        if (aliveCount_ >= MAX_PARTICLES) {
            return;
        }

        spawnAccumulator_ += settings_.spawnRate * dt;
        int toSpawn = std::min((int)MAX_PARTICLES - (int)aliveCount_, static_cast<int>(spawnAccumulator_));
        spawnAccumulator_ -= toSpawn;

        for (int i = 0; i < toSpawn; ++i) {
            pool_.positions[aliveCount_] = position + ImVec2 {
                rng_.range(-0.1F, 0.1F),
                rng_.range(-0.1F, 0.1F)
            };
            rng_.unitVector(pool_.velocities[aliveCount_].x, pool_.velocities[aliveCount_].y);
            pool_.velocities[aliveCount_] *= rng_.range(0.1f, 1.5f);
            pool_.ages[aliveCount_]        = 0;

            ++aliveCount_;
        }
    }

    void update(float dt)
    {
        if (settings_.enabled == false) {
            return;
        }


        for (std::size_t i {0}; i < aliveCount_; ++i) {
            pool_.positions[i] += pool_.velocities[i] * dt;
        }


        std::size_t i = 0;

        while (i < aliveCount_) {
            pool_.ages[i] += dt;
            if (pool_.ages[i] >= settings_.maxAge) {
                --aliveCount_;

                pool_.positions[i] = pool_.positions[aliveCount_];
                pool_.ages[i]      = pool_.ages[aliveCount_];
            }
            else {
                ++i;
            }
        }
    }

    void renderSettings()
    {
        ImGui::Begin("Emitter Settings");

        ImGui::Text("Active particles: %zu", aliveCount_);
        ImGui::Checkbox("Update", &settings_.enabled);
        ImGui::SliderFloat("Spawn rate", &settings_.spawnRate, 0.0F, MAX_PARTICLES, "%.0f /sec");
        ImGui::SliderFloat("Lifetime", &settings_.maxAge, 0.1F, 100.0F, "%.2f s");

        if (ImGui::Button("Kill")) {
            aliveCount_ = 0;
        }

        ImGui::End();
    }

private:

    std::size_t aliveCount_ {0};
    // std::array<Particle, MAX_PARTICLES> pool_ {};
    ParticlePool pool_;

    struct
    {
        bool enabled    = {true};
        float maxAge    = {10.0F};
        float spawnRate = {2000.0F};
    } settings_;

    double spawnAccumulator_ {};
    RNG    rng_;
};

#endif // YARR_EMITTER_HPP
