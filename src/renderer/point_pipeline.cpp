#include "point_pipeline.hpp"

#include "utils/shader_cache.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

PointPipeline::PointPipeline()
{
    program_ = ShaderCache::getProgram("PointProgram");

    viewProjLoc_       = glGetUniformLocation(program_, "viewProj");
    viewRowZLoc_       = glGetUniformLocation(program_, "viewRowZ");
    depthReferenceLoc_ = glGetUniformLocation(program_, "depthReference");
    pointSizeLoc_      = glGetUniformLocation(program_, "pointSize");
    attenuateLoc_      = glGetUniformLocation(program_, "attenuate");
    colorLoc_          = glGetUniformLocation(program_, "particleColor");
    roundPointsLoc_    = glGetUniformLocation(program_, "roundPoints");
    softnessLoc_       = glGetUniformLocation(program_, "softness");

    // Attribute format is fixed for the life of the VAO; only the buffer it reads from
    // changes, and that is rebound per frame as the ring advances.
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glEnableVertexAttribArray(0);
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glBindVertexArray(0);
}

PointPipeline::~PointPipeline()
{
    glDeleteVertexArrays(1, &vao_);
}

void PointPipeline::draw(ParticleBuffer const& particles, RenderView const& view)
{
    GLuint count = particles.count();
    if (count == 0) {
        return;
    }

    // Set rather than assumed: ImGui's backend leaves its own blend state behind, and
    // the other pipeline wants different state again.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, additive_? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE); // hands gl_PointSize to the vertex shader

    glUseProgram(program_);
    glUniformMatrix4fv(viewProjLoc_, 1, GL_FALSE, glm::value_ptr(view.viewProj));
    glUniform4fv(viewRowZLoc_, 1, glm::value_ptr(view.viewRowZ));
    glUniform1f(depthReferenceLoc_, view.depthReference);
    glUniform1f(pointSizeLoc_, pointSize_);
    glUniform1i(attenuateLoc_, static_cast<GLint>(attenuate_));
    glUniform4fv(colorLoc_, 1, glm::value_ptr(particleColor_));
    glUniform1i(roundPointsLoc_, static_cast<GLint>(roundPoints_));
    glUniform1f(softnessLoc_, softness_);

    glBindVertexArray(vao_);
    glBindVertexBuffer(0, particles.buffer(), 0, ParticleBuffer::stride());
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(count));
    glBindVertexArray(0);
}

void PointPipeline::renderSettings()
{
    ImGui::Begin("Points");

    ImGui::ColorEdit4("Color", glm::value_ptr(particleColor_), ImGuiColorEditFlags_AlphaBar);
    ImGui::SetItemTooltip("Alpha is per-particle weight — low values let density build up gradually");

    ImGui::SliderFloat("Point size", &pointSize_, 1.0F, 32.0F, "%.1f px");
    ImGui::Checkbox("Scale with distance", &attenuate_);
    ImGui::SetItemTooltip("Nominal size applies at the orbit target; nearer points grow, farther ones shrink");

    ImGui::Checkbox("Round", &roundPoints_);
    if (roundPoints_) {
        ImGui::SliderFloat("Softness", &softness_, 0.01F, 1.0F);
        ImGui::SetItemTooltip("Fraction of the radius spent fading out. 1 is a pure radial gradient.");
    }

    ImGui::Checkbox("Additive", &additive_);
    ImGui::SetItemTooltip(
        "Additive needs no depth sorting, so overlapping particles are order independent.\n"
        "Alpha blending does need it, and nothing here sorts — expect popping.");

    ImGui::End();
}
