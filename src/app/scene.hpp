#ifndef YARR_SCENE_HPP
#define YARR_SCENE_HPP

#include "emitter/emitter.hpp"
#include "logic/objects/attractor.hpp"

#include <glm/ext/vector_float2.hpp>
#include <memory>
#include <span>
#include <vector>

class Scene
{
public:
    void update(float dt);
    void renderSettings();

    void spawn(float dt) { emitter_->spawn(dt); }

    void placeEmitter(glm::vec2 position) { attractor_->setPosition(position); }

    [[nodiscard]] std::span<const ParticleVector> positions() const noexcept { return emitter_->data(); }
    [[nodiscard]] float const*                    ages() const noexcept      { return emitter_->ages(); }
    [[nodiscard]] float                           elapsed() const noexcept   { return static_cast<float>(elapsed_); }

    [[nodiscard]] std::vector<SceneObject const*> getSceneObjects() const noexcept
    {
        return {&emitter_->object(), &attractor_->object()};
    }

private:
    std::unique_ptr<Emitter>   emitter_ {std::make_unique<Emitter>()};
    std::unique_ptr<Attractor> attractor_ {std::make_unique<Attractor>()};
    double                     elapsed_ {0.0};
};

#endif // YARR_SCENE_HPP
