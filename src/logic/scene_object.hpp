#ifndef YARR_LOGIC_SCENE_OBJECT_HPP
#define YARR_LOGIC_SCENE_OBJECT_HPP

#include <glm/ext/vector_float3.hpp>

// A body in the scene, described by no more than where it is and how big it is. Its own
// header so a draw pipeline can consume it without pulling in whatever owns it.
struct SceneObject
{
    glm::vec3 position {0.0F, 0.0F, 0.0F};
    float     radius {0.25F};
};

#endif // YARR_LOGIC_SCENE_OBJECT_HPP
