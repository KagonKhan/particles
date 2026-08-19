#ifndef YARR_EMITTER_HPP
#define YARR_EMITTER_HPP

#include "logic/particle_pool.hpp"
#include "logic/scene_object.hpp"
#include "utils/rng.hpp"

#include <glm/ext/vector_float2.hpp>

#include <cstddef>


class Emitter
{
public:
    Emitter()
    {
        object_.attached = this;
    }

    void spawn(ParticlePool& pool, float dt);
    void reap(ParticlePool& pool, float dt);

    void renderSettings(ParticlePool& pool);

    void                             setPosition(glm::vec2 position) noexcept { object_.transform.position = position; }
    [[nodiscard]] bool               isEnabled() const noexcept               { return emittingSettings_.enabled; }
    [[nodiscard]] bool               isVisible() const noexcept               { return object_.visible; }
    [[nodiscard]] SceneObject const& object() const noexcept                  { return object_; }
    [[nodiscard]] SceneObject&       object() noexcept                        { return object_; }


private:
    struct EmittingSettings
    {
        bool enabled             = {true};
        float maxAge             = {10.0F};
        float spawnRate          = {2'000.0F};
        std::size_t maxParticles = {2'000'000};
    } emittingSettings_;

    SceneObject object_ {
        . transform   = {},
        .shape        = Circle {},
        .height       = 0.25F,
        .color        = {0.30F, 0.70F, 0.45F, 1.0F},
        .visible      = true,
        .name         = "Emitter",
        .attachedType = SceneObject::AttachedType::EMITTER,
    };

    double spawnAccumulator_ {};
    Rng    rng_;
};

#endif // YARR_EMITTER_HPP
