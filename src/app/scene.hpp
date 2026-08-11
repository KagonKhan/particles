#ifndef YARR_SCENE_HPP
#define YARR_SCENE_HPP

#include "emitter/emitter.hpp"

#include <memory>
#include <span>

// Everything that exists independently of how it is viewed: the particles themselves
// and how long they have been simulated. The renderer contributes the camera and the GL
// passes, so nothing here knows a projection exists.
class Scene
{
public:
    void update(float dt);
    void renderSettings();

    void spawn(float dt) { emitter_->spawn(dt); }

    [[nodiscard]] std::span<const ParticleVector> positions() const noexcept { return emitter_->data(); }
    [[nodiscard]] float const*                    ages() const noexcept      { return emitter_->ages(); }
    [[nodiscard]] float                           elapsed() const noexcept   { return static_cast<float>(elapsed_); }

    [[nodiscard]] std::vector<SceneObject const*> getSceneObjects() const noexcept
    {
        if (emitter_->isVisible()) { return {&emitter_->object()}; }

        return {};
    }

private:
    std::unique_ptr<Emitter> emitter_ {std::make_unique<Emitter>()};
    double                   elapsed_ {0.0};
};

#endif // YARR_SCENE_HPP
