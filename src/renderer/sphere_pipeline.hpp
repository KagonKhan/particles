#ifndef YARR_SPHERE_PIPELINE_HPP
#define YARR_SPHERE_PIPELINE_HPP

#include "logic/scene_object.hpp"
#include "renderer/render_view.hpp"

#include <GL/glew.h>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <span>

// Scene bodies as screen-facing impostors: one four-vertex quad per object, with the
// fragment shader rebuilding the sphere's normal across it. A position and a radius is
// all the input there is, so no mesh gets generated and nothing is uploaded per frame.
//
// The quad is flat, so the depth it writes is the depth of the sphere's centre plane
// rather than of its surface. That only shows up where two bodies interpenetrate.
class SpherePipeline
{
public:
    SpherePipeline();
    ~SpherePipeline();

    // Holds raw GL names, so copying one would double-free them.
    SpherePipeline(SpherePipeline const&)            = delete;
    SpherePipeline& operator=(SpherePipeline const&) = delete;
    SpherePipeline(SpherePipeline&&)                 = delete;
    SpherePipeline& operator=(SpherePipeline&&)      = delete;

    void draw(std::span<SceneObject const* const> objects, RenderView const& view);

    void renderSettings();

private:
    GLuint program_ {0};
    GLuint vao_ {0};

    GLint viewProjLoc_ {-1};
    GLint centerLoc_ {-1};
    GLint radiusLoc_ {-1};
    GLint camRightLoc_ {-1};
    GLint camUpLoc_ {-1};
    GLint colorLoc_ {-1};
    GLint lightDirLoc_ {-1};

    glm::vec4 color_ {0.35F, 0.45F, 0.75F, 1.0F};
    glm::vec3 lightDir_ {0.4F, 0.8F, 0.45F};
};

#endif // YARR_SPHERE_PIPELINE_HPP
