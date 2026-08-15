#include "scene.hpp"

void Scene::update(float dt)
{
    elapsed_ += dt;

    if (!emitter_->isEnabled()) {
        return;
    }

    attractor_->update(emitter_->data(), emitter_->velocity(), dt);
    emitter_->update(dt); // TODO: particles should be updated inside a scene instead.

    // After the integration, so a particle is resolved where it actually ended up rather
    // than where it was a step ago.
    boundary_->update(emitter_->getPool());
}

void Scene::renderSettings()
{
    emitter_->renderSettings();
    attractor_->renderSettings();
    boundary_->renderSettings();
}
