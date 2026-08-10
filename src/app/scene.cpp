#include "scene.hpp"

void Scene::update(float dt)
{
    elapsed_ += dt;
    emitter_->update(dt);
}

void Scene::renderSettings()
{
    emitter_->renderSettings();
}
