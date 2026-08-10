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
#include <span>


static constexpr std::size_t MAX_PARTICLES = 100'000'000;


using ParticleVector = glm::vec3;

// Where and along which axes a burst is emitted. The emitter stays ignorant of the
// camera; the caller supplies the basis, which is what lets a flat view emit a flat
// burst without the emitter knowing a projection exists.
struct SpawnFrame
{
    glm::vec3 origin {0.0F, 0.0F, 0.0F};
    glm::vec3 right {1.0F, 0.0F, 0.0F};
    glm::vec3 up {0.0F, 1.0F, 0.0F};

    // When set, jitter and velocity are confined to the right/up plane, so particles
    // have no motion at all along its normal.
    bool planar {false};
};

struct ParticlePool
{
    std::array<ParticleVector, MAX_PARTICLES> positions {};
    std::array<ParticleVector, MAX_PARTICLES> velocities {};
    std::array<float, MAX_PARTICLES> ages {};
};


class Emitter
{
public:
    void spawn(SpawnFrame const& frame, float dt);
    void update(float dt);
    void renderSettings();


    [[nodiscard]] std::span<const ParticleVector> data() const noexcept
    {
        return {pool_.positions.data(), aliveCount_};
    }

    [[nodiscard]] float const* ages() const noexcept { return pool_.ages.data(); }


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
