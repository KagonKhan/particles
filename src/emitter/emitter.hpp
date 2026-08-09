#ifndef YARR_EMITTER_HPP
#define YARR_EMITTER_HPP

#include "utils/rng.hpp"

#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstddef>


static constexpr std::size_t MAX_PARTICLES = 100'000'000;


using ParticleVector = glm::vec3;

struct ParticlePool
{
    std::array<ParticleVector, MAX_PARTICLES> positions {};
    std::array<ParticleVector, MAX_PARTICLES> velocities {};
    std::array<float, MAX_PARTICLES> ages {};
};


class Emitter
{
public:
    void spawn(glm::vec2 position, float dt);
    void update(float dt);
    void renderSettings();


    [[nodiscard]] std::size_t           aliveCount() const noexcept { return aliveCount_; }
    [[nodiscard]] ParticleVector const* data() const noexcept       { return pool_.positions.data(); }


    // std::pair<ImVec2 const*, std::size_t> culled(float cell_size);
private:

    std::size_t aliveCount_ {0};
    // std::array<ImVec2, MAX_PARTICLES> culled_ {};
    ParticlePool pool_ {};

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
