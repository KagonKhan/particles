#include "scene.hpp"

void Scene::update(float dt)
{
    elapsed_ += dt;

    if (!emitter_->isEnabled()) {
        return;
    }

    attractor_->update(*pool_, dt);
    emitter_->update(*pool_, dt);

    // After the integration, so a particle is resolved where it actually ended up rather
    // than where it was a step ago.
    boundary_->update(*pool_);

    // Last, so a particle spawns exactly where the emitter is and is drawn there before
    // anything has had a chance to move it.
    emitter_->spawn(*pool_, dt);
}

void Scene::renderSettings()
{
    emitter_->renderSettings(*pool_);
    attractor_->renderSettings();
    boundary_->renderSettings();
}
