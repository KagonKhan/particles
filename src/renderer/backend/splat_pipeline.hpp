#ifndef YARR_SPLAT_PIPELINE_HPP
#define YARR_SPLAT_PIPELINE_HPP

#include "renderer/backend/particle_buffer.hpp"
#include "renderer/render_view.hpp"
#include "utils/logger.hpp"

#include <GL/glew.h>

// Particles -> density image -> color, in two GPU passes:
//
//   splat   compute, one thread per particle, atomically adding a weighted disc into an
//           R32UI image. No sorting, no blending, no depth buffer — additive density is
//           order independent, which is what lets this scale to tens of millions.
//   resolve one fullscreen triangle turning accumulated density into a color.
//
// The heavy path: it survives counts that make the point rasterizer give up, and it can
// express density that overlapping sprites cannot. Reach for PointPipeline first.
class SplatPipeline : private Logger<SplatPipeline>
{
public:
    SplatPipeline();
    ~SplatPipeline();

    // Holds raw GL names, so copying one would double-free them.
    SplatPipeline(SplatPipeline const&)            = delete;
    SplatPipeline& operator=(SplatPipeline const&) = delete;
    SplatPipeline(SplatPipeline&&)                 = delete;
    SplatPipeline& operator=(SplatPipeline&&)      = delete;

    void draw(ParticleBuffer const& particles, RenderView const& view, int width, int height);

    void renderSettings();

private:
    void resizeDensityTexture(int width, int height);

    GLuint splatProgram_ {0};   // compute: particle positions -> density image
    GLuint resolveProgram_ {0}; // fullscreen: density image -> color
    GLuint fullscreenVAO_ {0};  // empty VAO, fullscreen triangle uses gl_VertexID
    GLuint densityTexture_ {0};

    // Splat pass uniforms
    GLint particleCountLoc_ {-1};
    GLint screenSizeLoc_ {-1};
    GLint viewProjLoc_ {-1};
    GLint depthFalloffLoc_ {-1};
    GLint depthReferenceLoc_ {-1};
    GLint viewRowZLoc_ {-1};
    GLint particleRadiusLoc_ {-1};

    // Resolve pass uniforms
    GLint densitySamplerLoc_ {-1};
    GLint colorLoc_ {-1};
    GLint fadeLoc_ {-1};

    int texW_ {0};
    int texH_ {0};

    // Per-dimension dispatch cap the driver reports. The spec floor is 65535, which is
    // also exactly what D3D12-backed drivers give, and this system routinely wants more
    // groups than that — see the dispatch in draw().
    GLuint maxWorkGroups_ {65535};

    float     fadeScale_ {0.15F};
    float     depthFalloff_ {1.5F};
    glm::vec4 particleColor_ {1.0F, 0.6F, 0.2F, 1.0F};

    // Splat radius in pixels. Every pixel of the disc is an atomic add, so the cost is
    // quadratic here and linear in the particle count.
    int particleRadius_ {1};
};

#endif // YARR_SPLAT_PIPELINE_HPP
