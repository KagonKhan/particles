#ifndef YARR_EMITTER_HPP
#define YARR_EMITTER_HPP

#include "logic/scene_object.hpp"
#include "utils/rng.hpp"

#include <glm/ext/vector_float2.hpp>

#include <array>
#include <cstddef>
#include <span>


static constexpr std::size_t MAX_PARTICLES = 100'000;


// The simulation is flat: a particle has a position and a velocity in the plane, and
// nothing here can take it out of that plane. The camera is free to look at the plane
// from wherever it likes — that is the renderer's business, not the emitter's.
using ParticleVector = glm::vec2;

class Emitter
{
public:
    void spawn(float dt);
    void update(float dt);
    void renderSettings();

    [[nodiscard]]
    std::span<const ParticleVector> data() const noexcept { return {pool_.positions.data(), aliveCount_}; }
    [[nodiscard]] float const*      ages() const noexcept { return pool_.ages.data(); }

    [[nodiscard]] bool               isVisible() const noexcept { return emitterSettings_.visible; }
    [[nodiscard]] SceneObject const& object() const noexcept    { return emitterSettings_.object; }

    void setPosition(glm::vec2 position) noexcept { emitterSettings_.object.position = position; }

private:

    std::size_t aliveCount_ {0};

    struct ParticlePool
    {
        std::array<ParticleVector, MAX_PARTICLES> positions {};
        std::array<ParticleVector, MAX_PARTICLES> velocities {};
        std::array<float, MAX_PARTICLES> ages {};
    } pool_ {};

    struct EmittingSettings
    {
        bool enabled             = {true};
        float maxAge             = {10.0F};
        float spawnRate          = {2'000.0F};
        std::size_t maxParticles = {10'000};
    } emittingSettings_;

    struct EmitterSettings
    {
        SceneObject object;
        bool visible {true};
    } emitterSettings_;

    double spawnAccumulator_ {};
    RNG    rng_;
};

#endif // YARR_EMITTER_HPP
