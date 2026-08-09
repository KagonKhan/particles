#ifndef YARR_RENDERER_HPP
#define YARR_RENDERER_HPP

#include "emitter/emitter.hpp"
#include <GL/glew.h> // or whatever GL loader you use
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>

// Cosine gradient (color = a + b*cos(2pi*(c*t + d))) driven by particle spawn time.
// The defaults sweep deep blue -> pale blue-white -> rose -> plum, the palette real
// colored space imagery tends to land in.
struct NebulaPalette
{
    glm::vec3 a {0.55F, 0.50F, 0.68F}; // midpoint
    glm::vec3 b {0.35F, 0.30F, 0.32F}; // amplitude
    glm::vec3 c {1.00F, 1.00F, 1.00F}; // frequency
    glm::vec3 d {0.55F, 0.62F, 0.78F}; // phase

    float cycleRate {0.08F}; // full color revolutions per second of spawning
    float mix {1.0F};        // 0 = flat particleColor, 1 = full gradient
    float coreWhiten {0.6F}; // how much the densest cores blow out toward white
};

// Orbits the origin; the particle cloud lives in roughly [-1,1]^3 world space.
struct Camera
{
    float yaw {0.0F};      // radians, around the world Y axis
    float pitch {0.2F};    // radians, above the XZ plane
    float distance {3.5F}; // from the orbit target
    float fovDegrees {60.0F};

    [[nodiscard]] glm::vec3 forward() const noexcept;
    [[nodiscard]] glm::vec3 eye() const noexcept;
    [[nodiscard]] glm::mat4 viewProj(float aspect) const noexcept;
};

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void render(GLFWwindow* window);

private:
    void resizeDensityTexture(int w, int h);

    GLint pointSizeLoc_ {-1}; // kept for compatibility, unused now
    GLint colorLoc_ {-1};
    GLint splatColorLoc_ {-1};
    GLint particleCountLoc_ {-1};
    GLint screenSizeLoc_ {-1};
    GLint viewProjLoc_ {-1};
    GLint depthFalloffLoc_ {-1};
    GLint depthReferenceLoc_ {-1};
    GLint elapsedTimeLoc_ {-1};
    GLint cycleRateLoc_ {-1};

    GLint densitySamplerLoc_ {-1};
    GLint hueSamplerLoc_ {-1};
    GLint fadeLoc_ {-1};
    GLint paletteALoc_ {-1};
    GLint paletteBLoc_ {-1};
    GLint paletteCLoc_ {-1};
    GLint paletteDLoc_ {-1};
    GLint colorMixLoc_ {-1};
    GLint coreWhitenLoc_ {-1};

    int texW_ {0};
    int texH_ {0};

    double        elapsedTime_ {0.0};
    Camera        camera_;
    NebulaPalette palette_;
    Emitter       emitter;
};

#endif // YARR_RENDERER_HPP
