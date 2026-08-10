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

    void spawn(SpawnFrame const& frame, float dt) { emitter_->spawn(frame, dt); }

    [[nodiscard]] std::span<const ParticleVector> positions() const noexcept { return emitter_->data(); }

    // Ages are swap-removed alongside positions, so index i refers to the same particle
    // in both.
    [[nodiscard]] float const* ages() const noexcept { return emitter_->ages(); }
    [[nodiscard]] float        elapsed() const noexcept { return static_cast<float>(elapsed_); }

private:
    // The pool is a couple of gigabytes wide (see MAX_PARTICLES), so an Emitter can only
    // ever live on the heap — a value member here would blow the stack of whoever owns
    // the Scene.
    std::unique_ptr<Emitter> emitter_ {std::make_unique<Emitter>()};

    // Double, because this only grows: a float loses sub-frame resolution within an hour
    // of running.
    double elapsed_ {0.0};
};

#endif // YARR_SCENE_HPP
