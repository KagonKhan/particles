#ifndef YARR_SCENE_HPP
#define YARR_SCENE_HPP

#include "emitter/emitter.hpp"
#include "logic/objects/attractor.hpp"
#include "logic/objects/boundary.hpp"
#include "logic/particle_pool.hpp"

#include <glm/ext/vector_float2.hpp>
#include <memory>
#include <span>
#include <vector>

class Scene
{
public:
    // One simulation step. The caller decides how often a step happens and how long it
    // lasts — dt here is the simulation's, not the frame's.
    void update(float dt);
    void renderSettings();

    void placeEmitter(glm::vec2 position) { attractor_->setPosition(position); }

    [[nodiscard]] std::span<const ParticleVector> positions() const noexcept { return pool_->alivePositions(); }
    [[nodiscard]] float const*                    ages() const noexcept      { return pool_->ages.data(); }
    [[nodiscard]] float                           elapsed() const noexcept   { return static_cast<float>(elapsed_); }

    [[nodiscard]] std::vector<SceneObject const*> getSceneObjects() const noexcept
    {
        return {&emitter_->object(), &attractor_->object(), &boundary_->object()};
    }

private:
    // On the heap: the pool runs to tens of megabytes, and Scene is a value member of a
    // stack-allocated App.
    std::unique_ptr<ParticlePool> pool_ {std::make_unique<ParticlePool>()};

    std::unique_ptr<Emitter>   emitter_ {std::make_unique<Emitter>()};
    std::unique_ptr<Attractor> attractor_ {std::make_unique<Attractor>()};
    std::unique_ptr<Boundary>  boundary_ {std::make_unique<Boundary>()};
    double                     elapsed_ {0.0};
};

#endif // YARR_SCENE_HPP
