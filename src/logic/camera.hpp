#ifndef YARR_LOGIC_CAMERA_HPP
#define YARR_LOGIC_CAMERA_HPP

#include "renderer/render_view.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <cstdint>
#include <numbers>
enum class Projection : std::uint8_t
{
    Perspective,  // 3D
    Orthographic, // 2D
};

// A free camera: it has a position in the world and a direction it faces, and neither is
// tied to anything in the scene. Movement keys push the position around; nothing orbits.
struct Camera
{
    // Flat by default, to match the simulation: the cloud lives in a plane, so a parallel
    // projection square on to it is the honest way to look at it. Perspective is still one
    // click away, and a free camera makes it worth having.
    Projection projection {Projection::Orthographic};

    // On the +z side of the simulation plane, looking back down -z at it. Which side
    // matters now that the world is flat: seen from -z the plane is mirrored, because
    // lookAt's screen-right axis is cross(forward, worldUp) and that puts world +x on the
    // left. From here +x is screen-right and +y is up, which is what the emitter's
    // position sliders and drag-placement read as.
    glm::vec3 position {0.0F, 0.0F, 4.0F};

    float yaw {std::numbers::pi_v<float>}; // radians, around the world Y axis; pi faces -z
    float pitch {0.0F};                    // radians, above the horizon
    float fovDegrees {60.0F};

    // How far ahead of the camera the plane of interest sits. Nothing about the view
    // depends on it — it sets the 2D zoom, the distance at which a particle draws at its
    // nominal size, and the depth a click spawns at.
    float focusDistance {4.0F};

    [[nodiscard]] glm::vec3 forward() const noexcept;
    [[nodiscard]] glm::vec3 right() const noexcept;
    [[nodiscard]] glm::vec3 up() const noexcept;
    [[nodiscard]] glm::vec3 eye() const noexcept;
    [[nodiscard]] glm::vec3 focusPoint() const noexcept; // position, focusDistance ahead
    [[nodiscard]] glm::mat4 view() const noexcept;
    [[nodiscard]] glm::mat4 viewProj(float aspect) const noexcept;

    // Depth at which a particle should render at its nominal size and brightness.
    // Tracks the focus distance in 3D; in 2D it tracks the fixed standoff, which leaves
    // the depth weighting near-flat — the right answer for a parallel projection.
    [[nodiscard]] float depthReference() const noexcept;

    [[nodiscard]] RenderView renderView(float aspect) const noexcept;
};

#endif // YARR_LOGIC_CAMERA_HPP
