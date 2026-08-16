#ifndef YARR_RENDERER_HPP
#define YARR_RENDERER_HPP

#include "app/settings.hpp"
#include "renderer/backend/particle_buffer.hpp"
#include "renderer/backend/point_pipeline.hpp"
#include "renderer/backend/shape_pipeline.hpp"
#include "renderer/backend/splat_pipeline.hpp"
#include "renderer/camera.hpp"
#include "renderer/render_view.hpp"
#include "utils/shader_cache.hpp"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <GL/glew.h> // or whatever GL loader you use
#include <cstdint>

class Scene;
class Simulation;

enum class RenderMode : std::uint8_t
{
    POINTS, // one GL_POINTS draw, the plain path
    SPLAT,  // compute-accumulated density, the heavy path
};

class Renderer
{
public:
    void render(FramebufferSize size, Simulation& simulation, float dt);

private:
    void renderSettings(float dt);
    void renderCameraPanel();
    void dragEmitter(Scene& scene, glm::mat4 const& view_proj, int w, int h);

    // First, and deliberately: every pipeline below looks its program up in its constructor.
    DefaultShaders shaders_;

    Camera     camera_;
    RenderMode mode_ {RenderMode::POINTS};

    // One upload per frame, shared by both pipelines: at MAX_PARTICLES the storage runs
    // to gigabytes, so a private copy per pipeline is not on the table.
    ParticleBuffer particles_;
    PointPipeline  points_;
    SplatPipeline  splat_;

    // Scene bodies. Independent of the particle mode — both particle paths draw over it.
    ShapePipeline shapes_;

    // Sweeps yaw where the camera stands, which is a free camera's version of the orbit
    // this used to do.
    bool autoTurn_ {false};

    // Latched on the right-button press, so a look that drags across a panel is not cut
    // short by ImGui claiming the mouse partway through.
    bool mouseLooking_ {false};

    // Only the panels. Camera input keeps running with them hidden, so flying the view
    // around does not depend on anything being on screen.
    Knob<bool>& showRenderer_ {Settings::getInstance().option<bool>("View", "Renderer", true)};
    Knob<bool>& showCamera_ {Settings::getInstance().option<bool>("View", "Camera", true)};
};

#endif // YARR_RENDERER_HPP
