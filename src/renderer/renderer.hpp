#ifndef YARR_RENDERER_HPP
#define YARR_RENDERER_HPP

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <GL/glew.h> // or whatever GL loader you use
#include <GLFW/glfw3.h>
#include <cstdint>

class Scene;

enum class Projection : std::uint8_t
{
    Perspective,  // 3D
    Orthographic, // 2D — parallel projection, no foreshortening
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

    // Depth at which a particle should render at full brightness. Tracks the orbit
    // distance in 3D; in 2D it tracks the fixed standoff, which leaves the depth
    // weighting near-flat — the physically right answer for a parallel projection.
    [[nodiscard]] float depthReference() const noexcept;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    // Draws whatever the scene currently holds, and feeds mouse spawns back into it —
    // the camera basis a burst is emitted along only exists on this side.
    void render(GLFWwindow* window, Scene& scene, float dt);

private:
    void resizeDensityTexture(int w, int h);
    void renderSettings(float dt);
    void spawnFromMouse(Scene& scene, glm::mat4 const& viewProj, int w, int h, float dt);

    // Splat pass
    GLint particleCountLoc_ {-1};
    GLint screenSizeLoc_ {-1};
    GLint viewProjLoc_ {-1};
    GLint depthFalloffLoc_ {-1};
    GLint depthReferenceLoc_ {-1};
    GLint viewRowZLoc_ {-1};
    GLint particleRadiusLoc_ {-1};

    // Resolve pass
    GLint densitySamplerLoc_ {-1};
    GLint colorLoc_ {-1};
    GLint fadeLoc_ {-1};

    int texW_ {0};
    int texH_ {0};

    // Per-dimension dispatch cap the driver reports. The spec floor is 65535, which is
    // also exactly what D3D12-backed drivers give, and this system routinely wants more
    // groups than that — see the dispatch in render().
    GLuint maxWorkGroups_ {65535};

    Camera camera_;

    // Emission is confined to the plane facing the camera, so it follows the projection
    // by default but stays independently overridable.
    bool planarEmission_ {false};
    bool autoOrbit_ {false};

    float     fadeScale_ {0.15F};
    float     depthFalloff_ {1.5F};
    glm::vec4 particleColor_ {1.0F, 0.6F, 0.2F, 1.0F};

    // Splat radius in pixels. Every pixel of the disc is an atomic add, so the cost is
    // quadratic here and linear in the particle count.
    int particleRadius_ {1};
};

#endif // YARR_RENDERER_HPP
