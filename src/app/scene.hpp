#ifndef YARR_SCENE_HPP
#define YARR_SCENE_HPP

#include "emitter/emitter.hpp"

class Scene
{
public:
    void render(float dt);
    void update(float dt);

private:
    Emitter emitter;
};

#endif // YARR_SCENE_HPP
