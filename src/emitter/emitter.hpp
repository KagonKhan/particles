#ifndef YARR_EMITTER_HPP
#define YARR_EMITTER_HPP

#include "logic/scene_object.hpp"
#include "utils/rng.hpp"

#include <glm/ext/vector_float2.hpp>

#include <array>
#include <cstddef>
#include <span>


static constexpr std::size_t MAX_PARTICLES = 1'000'000;


// The simulation is flat: a particle has a position and a velocity in the plane, and
// nothing here can take it out of that plane. The camera is free to look at the plane
// from wherever it likes — that is the renderer's business, not the emitter's.
using ParticleVector = glm::vec2;
struct ParticlePool
{
    std::array<ParticleVector, MAX_PARTICLES> positions {};
    std::array<ParticleVector, MAX_PARTICLES> velocities {};
    std::array<float, MAX_PARTICLES> ages {};

    std::size_t aliveCount {};
};


class Emitter
{
public:
    void spawn(float dt);
    void update(float dt);
    void renderSettings();

    [[nodiscard]]
    std::span<ParticleVector>  data() noexcept       { return {pool_.positions.data(), pool_.aliveCount}; }
    std::span<ParticleVector>  velocity() noexcept   { return {pool_.velocities.data(), pool_.aliveCount}; }
    [[nodiscard]] float const* ages() const noexcept { return pool_.ages.data(); }

    // Whether the simulation is advancing. The scene owns the decision to skip a step, so
    // that everything acting on the pool is paused together rather than each in isolation.
    [[nodiscard]] bool               isEnabled() const noexcept { return emittingSettings_.enabled; }
    [[nodiscard]] bool               isVisible() const noexcept { return object_.visible; }
    [[nodiscard]] SceneObject const& object() const noexcept    { return object_; }

    void setPosition(glm::vec2 position) noexcept { object_.transform.position = position; }

    ParticlePool& getPool() noexcept { return pool_; }

private:

    ParticlePool pool_ {};

    struct EmittingSettings
    {
        bool enabled             = {true};
        float maxAge             = {10.0F};
        float spawnRate          = {2'000.0F};
        std::size_t maxParticles = {1'000'000};
    } emittingSettings_;

    SceneObject object_ {
        . transform = {},
        .shape      = Circle {},
        .height     = 0.25F,
        .color      = {0.30F, 0.70F, 0.45F, 1.0F},
        .visible    = true,
    };

    double spawnAccumulator_ {};
    RNG    rng_;
};

#endif // YARR_EMITTER_HPP
