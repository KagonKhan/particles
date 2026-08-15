#ifndef YARR_EMITTER_HPP
#define YARR_EMITTER_HPP

#include "logic/particle_pool.hpp"
#include "logic/scene_object.hpp"
#include "utils/rng.hpp"

#include <glm/ext/vector_float2.hpp>

#include <cstddef>


// The pool arrives per call rather than being held: the scene owns the particles, and an
// emitter is only one of the things allowed to write into them.
class Emitter
{
public:
    void spawn(ParticlePool& pool, float dt);
    void update(ParticlePool& pool, float dt);
    void renderSettings(ParticlePool& pool);

    // Whether the simulation is advancing. The scene owns the decision to skip a step, so
    // that everything acting on the pool is paused together rather than each in isolation.
    [[nodiscard]] bool               isEnabled() const noexcept { return emittingSettings_.enabled; }
    [[nodiscard]] bool               isVisible() const noexcept { return object_.visible; }
    [[nodiscard]] SceneObject const& object() const noexcept    { return object_; }

    void setPosition(glm::vec2 position) noexcept { object_.transform.position = position; }

private:

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
