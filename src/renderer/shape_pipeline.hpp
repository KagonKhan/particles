#ifndef YARR_SHAPE_PIPELINE_HPP
#define YARR_SHAPE_PIPELINE_HPP

#include "logic/scene_object.hpp"
#include "renderer/render_view.hpp"

#include <GL/glew.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Every shape through one program: the scene goes up as per-instance attributes and comes
// out of one instanced draw of a four-vertex quad, with shape.frag ray marching the
// distance field for the surface. Nothing is meshed, so a body costs 48 bytes and adding
// a shape is a case in shape.frag rather than a pipeline.
class ShapePipeline
{
public:
    ShapePipeline();
    ~ShapePipeline();

    // Holds raw GL names, so copying one would double-free them.
    ShapePipeline(ShapePipeline const&)            = delete;
    ShapePipeline& operator=(ShapePipeline const&) = delete;
    ShapePipeline(ShapePipeline&&)                 = delete;
    ShapePipeline& operator=(ShapePipeline&&)      = delete;

    void draw(std::span<SceneObject const* const> objects, RenderView const& view);

    void renderSettings();

private:
    // All vec4, so the offsets the vertex array is set up with hold whatever alignment glm
    // is configured for. The shape type rides in params.w rather than as an integer of its
    // own, which would put the tail of the struct at the mercy of that alignment.
    struct GpuBody
    {
        glm::vec4 placement; //  0 — xy world centre, zw the footprint's half extents
        glm::vec4 params;    // 16 — xy dimensions, z height, w the shape type
        glm::vec4 color;     // 32
    };

    static_assert(sizeof(GpuBody) == 48);

    GLuint     program_ {0};
    GLuint     vao_ {0};
    GLuint     buffer_ {0};
    GLsizeiptr capacity_ {0}; // bytes the instance buffer currently holds

    GLint viewProjLoc_ {-1};
    GLint camRightLoc_ {-1};
    GLint camUpLoc_ {-1};
    GLint cameraEyeLoc_ {-1};
    GLint cameraForwardLoc_ {-1};
    GLint orthographicLoc_ {-1};
    GLint lightDirLoc_ {-1};
    GLint ambientLoc_ {-1};

    std::vector<GpuBody> bodies_;

    glm::vec3 lightDir_ {0.4F, 0.8F, 0.45F};
    float     ambient_ {0.15F};
};

#endif // YARR_SHAPE_PIPELINE_HPP
