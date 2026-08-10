#ifndef YARR_RENDERER_HPP
#define YARR_RENDERER_HPP

#include "renderer/particle_buffer.hpp"
#include "renderer/point_pipeline.hpp"
#include "renderer/render_view.hpp"
#include "renderer/splat_pipeline.hpp"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <GL/glew.h> // or whatever GL loader you use
#include <GLFW/glfw3.h>
#include <cstdint>

class Scene;

enum class Projection : std::uint8_t
{
    Perspective,  // 3D
    Orthographic, // 2D — parallel projection, no foreshortening
};

// Which pipeline gets to draw the particle buffer this frame.
enum class RenderMode : std::uint8_t
{
    Points, // one GL_POINTS draw, the plain path
    Splat,  // compute-accumulated density, the heavy path
};

// Orbits the origin, which is where the emitter's spawn plane sits.
struct Camera
{
    Projection projection {Projection::Perspective};

    float yaw {0.0F};      // radians, around the world Y axis
    float pitch {0.2F};    // radians, above the XZ plane
    float distance {3.5F}; // from the orbit target
    float fovDegrees {60.0F};

    [[nodiscard]] glm::vec3 forward() const noexcept;

    // Screen right and up in world space — the basis of the plane facing the camera.
    [[nodiscard]] glm::vec3 right() const noexcept;
    [[nodiscard]] glm::vec3 up() const noexcept;

    [[nodiscard]] glm::vec3 eye() const noexcept;
    [[nodiscard]] glm::mat4 view() const noexcept;
    [[nodiscard]] glm::mat4 viewProj(float aspect) const noexcept;

    // Depth at which a particle should render at its nominal size and brightness.
    // Tracks the orbit distance in 3D; in 2D it tracks the fixed standoff, which leaves
    // the depth weighting near-flat — the right answer for a parallel projection.
    [[nodiscard]] float depthReference() const noexcept;

    // Everything a pipeline needs from this camera, and nothing more.
    [[nodiscard]] RenderView renderView(float aspect) const noexcept;
};

// Points a camera at a scene, streams it to the GPU once, and hands it to whichever
// pipeline is active. Owns no GL state of its own beyond the shared particle buffer.
class Renderer
{
public:
    // Feeds mouse spawns back into the scene — the camera basis a burst is emitted
    // along only exists on this side.
    void render(GLFWwindow* window, Scene& scene, float dt);

private:
    void renderSettings(float dt);
    void spawnFromMouse(Scene& scene, glm::mat4 const& viewProj, int w, int h, float dt);

    Camera     camera_;
    RenderMode mode_ {RenderMode::Points};

    // One upload per frame, shared by both pipelines: at MAX_PARTICLES the storage runs
    // to gigabytes, so a private copy per pipeline is not on the table.
    ParticleBuffer particles_;
    PointPipeline  points_;
    SplatPipeline  splat_;

    // Emission is confined to the plane facing the camera, so it follows the projection
    // by default but stays independently overridable.
    bool planarEmission_ {false};
    bool autoOrbit_ {false};
};

#endif // YARR_RENDERER_HPP
