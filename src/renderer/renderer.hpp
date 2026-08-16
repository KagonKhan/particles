#ifndef YARR_RENDERER_HPP
#define YARR_RENDERER_HPP

#include "logic/camera.hpp"
#include "renderer/particle_buffer.hpp"
#include "renderer/point_pipeline.hpp"
#include "renderer/render_view.hpp"
#include "renderer/shape_pipeline.hpp"
#include "renderer/splat_pipeline.hpp"
#include "utils/shader_cache.hpp"

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <GL/glew.h> // or whatever GL loader you use
#include <cstdint>

class Scene;

enum class RenderMode : std::uint8_t
{
    Points, // one GL_POINTS draw, the plain path
    Splat,  // compute-accumulated density, the heavy path
};

class Renderer
{
public:
    Renderer() { ShaderCache::loadDefaults(); }
    ///@brief Draws one frame into a framebuffer of the given size. The size is passed in
    /// rather than queried, so nothing here has to know what kind of window it came from.
    void render(int width, int height, Scene& scene, float dt);

private:
    void renderSettings(float dt);
    void dragEmitter(Scene& scene, glm::mat4 const& viewProj, int w, int h);

    Camera     camera_;
    RenderMode mode_ {RenderMode::Points};

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
};

#endif // YARR_RENDERER_HPP
