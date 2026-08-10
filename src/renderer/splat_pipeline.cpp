#include "splat_pipeline.hpp"

#include "utils/shader_cache.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cstdint>

namespace
{

// One thread per particle; must match local_size_x in splat.comp.
constexpr GLuint kLocalSize = 256;

GLuint createUintTexture(int w, int h)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

} // namespace

SplatPipeline::SplatPipeline()
{
    splatProgram_   = ShaderCache::getProgram("SplatProgram");
    resolveProgram_ = ShaderCache::getProgram("ResolveProgram");

    particleCountLoc_  = glGetUniformLocation(splatProgram_, "particleCount");
    screenSizeLoc_     = glGetUniformLocation(splatProgram_, "screenSize");
    viewProjLoc_       = glGetUniformLocation(splatProgram_, "viewProj");
    depthFalloffLoc_   = glGetUniformLocation(splatProgram_, "depthFalloff");
    depthReferenceLoc_ = glGetUniformLocation(splatProgram_, "depthReference");
    viewRowZLoc_       = glGetUniformLocation(splatProgram_, "viewRowZ");
    particleRadiusLoc_ = glGetUniformLocation(splatProgram_, "particleRadius");

    densitySamplerLoc_ = glGetUniformLocation(resolveProgram_, "densityImage");
    colorLoc_          = glGetUniformLocation(resolveProgram_, "particleColor");
    fadeLoc_           = glGetUniformLocation(resolveProgram_, "fadeScale");

    std::array<GLint, 3> maxGroups {};
    for (GLuint dim = 0; dim < maxGroups.size(); ++dim) {
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, dim, &maxGroups[dim]);
    }

    maxWorkGroups_ = static_cast<GLuint>(maxGroups[0]);
    spdlog::info(
        "max compute work groups: {} x {} x {} ({} particles per dispatch axis)",
        maxGroups[0],
        maxGroups[1],
        maxGroups[2],
        static_cast<std::uint64_t>(maxWorkGroups_) * kLocalSize);

    // Fullscreen triangle needs no vertex data at all — gl_VertexID drives it.
    glGenVertexArrays(1, &fullscreenVAO_);
}

SplatPipeline::~SplatPipeline()
{
    glDeleteTextures(1, &densityTexture_);
    glDeleteVertexArrays(1, &fullscreenVAO_);
}

void SplatPipeline::resizeDensityTexture(int width, int height)
{
    if ((width == texW_) && (height == texH_)) {
        return;
    }

    texW_ = width;
    texH_ = height;

    if (densityTexture_ != 0) {
        glDeleteTextures(1, &densityTexture_);
    }

    densityTexture_ = createUintTexture(width, height);
}

void SplatPipeline::draw(ParticleBuffer const& particles, RenderView const& view, int width, int height)
{
    resizeDensityTexture(width, height);

    GLuint count = particles.count();

    static const GLuint zero = 0;
    glClearTexImage(densityTexture_, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    // --- Splat pass ---
    glUseProgram(splatProgram_);
    glUniform1ui(particleCountLoc_, count);
    glUniform2i(screenSizeLoc_, width, height);
    glUniformMatrix4fv(viewProjLoc_, 1, GL_FALSE, glm::value_ptr(view.viewProj));
    glUniform4fv(viewRowZLoc_, 1, glm::value_ptr(view.viewRowZ));
    glUniform1f(depthReferenceLoc_, view.depthReference);
    glUniform1f(depthFalloffLoc_, depthFalloff_);
    glUniform1i(particleRadiusLoc_, particleRadius_);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particles.buffer());
    glBindImageTexture(1, densityTexture_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

    GLuint groups = (count + kLocalSize - 1) / kLocalSize;
    if (groups > 0) {
        // A dispatch is capped per dimension — 65535 on any D3D12-backed driver, which
        // is only ~16.7M particles down a single axis. Past the cap the driver rejects
        // the whole dispatch with GL_INVALID_VALUE and splats nothing, so the screen
        // goes black instead of degrading. Folding the excess into Y buys 65535x room;
        // the shader linearizes the grid back into a particle index.
        GLuint groupsX = std::min(groups, maxWorkGroups_);
        GLuint groupsY = (groups + groupsX - 1) / groupsX;
        glDispatchCompute(groupsX, groupsY, 1);
    }

    // Make the atomic writes visible to the resolve pass, and to next frame's
    // glClearTexImage — image stores are incoherent in both directions.
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);

    // --- Resolve pass ---
    // Density already carries the accumulation, so this is a straight composite of one
    // translucent layer over the background. Set explicitly: ImGui's backend leaves its
    // own blend state behind, and the point pipeline wants different state again.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(resolveProgram_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // additive
    glUniform4fv(colorLoc_, 1, glm::value_ptr(particleColor_));
    glUniform1f(fadeLoc_, fadeScale_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture_);
    glUniform1i(densitySamplerLoc_, 0);

    glBindVertexArray(fullscreenVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void SplatPipeline::renderSettings()
{
    ImGui::Begin("Splat");

    ImGui::SliderFloat("Density fade", &fadeScale_, 0.01F, 1.0F);
    ImGui::SetItemTooltip("How quickly accumulated density saturates to full color");

    ImGui::ColorEdit4("Particle color", glm::value_ptr(particleColor_));

    ImGui::SliderInt("Particle size", &particleRadius_, 0, 8, "%d px radius");
    ImGui::SetItemTooltip(
        "0 = one pixel per particle.\n"
        "Cost is quadratic: every pixel of the disc is an atomic add,\n"
        "so radius 4 is ~50x the splat work of radius 0.");

    ImGui::SliderFloat("Depth falloff", &depthFalloff_, 0.0F, 3.0F);
    ImGui::SetItemTooltip(
        "0 = flat, 1 = 1/depth, 2 = inverse-square.\n"
        "Near-flat in 2D: a parallel projection has no distance attenuation.");

    ImGui::End();
}
