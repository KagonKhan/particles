#ifndef YARR_POINT_PIPELINE_HPP
#define YARR_POINT_PIPELINE_HPP

#include "renderer/particle_buffer.hpp"
#include "renderer/render_view.hpp"

#include <GL/glew.h>

// One GL_POINTS draw over the particle buffer. No accumulation, no intermediate targets
// — the fixed-function point rasterizer and a blend mode do all the work, which makes
// this the path to reach for when the question is about the simulation rather than the
// look.
class PointPipeline
{
public:
    PointPipeline();
    ~PointPipeline();

    // Holds raw GL names, so copying one would double-free them.
    PointPipeline(PointPipeline const&)            = delete;
    PointPipeline& operator=(PointPipeline const&) = delete;
    PointPipeline(PointPipeline&&)                 = delete;
    PointPipeline& operator=(PointPipeline&&)      = delete;

    void draw(ParticleBuffer const& particles, RenderView const& view);

    void renderSettings();

private:
    GLuint program_ {0};
    GLuint vao_ {0};

    GLint viewProjLoc_ {-1};
    GLint viewRowZLoc_ {-1};
    GLint depthReferenceLoc_ {-1};
    GLint pointSizeLoc_ {-1};
    GLint attenuateLoc_ {-1};
    GLint colorLoc_ {-1};
    GLint roundPointsLoc_ {-1};
    GLint softnessLoc_ {-1};

    glm::vec4 particleColor_ {1.0F, 0.6F, 0.2F, 0.5F};

    float pointSize_ {2.0F};
    bool  attenuate_ {true};

    bool  roundPoints_ {true};
    float softness_ {0.5F};

    // Additive is the honest default for glowing particles: it needs no depth sort,
    // because addition does not care what order the points arrive in. Alpha blending
    // does care, and this pipeline deliberately never sorts.
    bool additive_ {true};
};

#endif // YARR_POINT_PIPELINE_HPP
