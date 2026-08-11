#ifndef YARR_LOGIC_SCENE_OBJECT_HPP
#define YARR_LOGIC_SCENE_OBJECT_HPP

#include <glm/ext/vector_float2.hpp>

// A body in the scene, described by no more than where it is and how big it is. It lives
// in the same plane the particles do, so its position is flat too. Its own header so a
// draw pipeline can consume it without pulling in whatever owns it.
struct SceneObject
{
    glm::vec2 position {0.0F, 0.0F};
    float     radius {0.25F};
};

#endif // YARR_LOGIC_SCENE_OBJECT_HPP
