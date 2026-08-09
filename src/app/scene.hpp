#ifndef YARR_SCENE_HPP
#define YARR_SCENE_HPP

#include "emitter/emitter.hpp"

class Scene
{
public:
    void render(float dt)
    {
        ImGui::Begin("Scene");


        ImGui::End();
    }

    void update(float dt)
    {
        emitter.update(dt);
    }

private:
    Emitter emitter;
};

#endif // YARR_SCENE_HPP
