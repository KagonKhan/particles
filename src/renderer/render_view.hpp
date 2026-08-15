#ifndef YARR_RENDER_VIEW_HPP
#define YARR_RENDER_VIEW_HPP

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

// Everything a draw pipeline needs from whatever is looking at the scene. Pipelines stay
// ignorant of the camera itself, the same way the emitter stays ignorant of it — the
// caller owns the projection and hands over only its results.
struct RenderView
{
    glm::mat4 viewProj {1.0F};

    // Third row of the view matrix, which is all that is needed to recover a point's
    // distance along the camera's forward axis. Depth comes from this rather than from
    // clip.w so one shader serves both projections: clip.w is the depth only under
    // perspective, and is pinned at 1.0 under orthographic.
    glm::vec4 viewRowZ {0.0F, 0.0F, -1.0F, 0.0F};

    float depthReference {1.0F}; // depth at which a particle draws at its nominal size

    // World-space camera basis, unit length. Screen-facing geometry is built from it, so
    // a pipeline can face the camera without ever being handed the camera.
    glm::vec3 right {1.0F, 0.0F, 0.0F};
    glm::vec3 up {0.0F, 1.0F, 0.0F};

    // Where the view rays come from, for a pipeline that traces them. Under perspective
    // they all leave the eye and forward is only the centre of the fan; under an
    // orthographic projection they run parallel along forward and the eye is a standoff.
    glm::vec3 eye {0.0F, 0.0F, 1.0F};
    glm::vec3 forward {0.0F, 0.0F, -1.0F};
    bool      orthographic {false};
};

#endif // YARR_RENDER_VIEW_HPP
