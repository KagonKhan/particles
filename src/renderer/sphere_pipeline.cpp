#include "sphere_pipeline.hpp"

#include "utils/shader_cache.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

SpherePipeline::SpherePipeline()
{
    program_ = ShaderCache::getProgram("SphereProgram");

    viewProjLoc_ = glGetUniformLocation(program_, "viewProj");
    centerLoc_   = glGetUniformLocation(program_, "center");
    radiusLoc_   = glGetUniformLocation(program_, "radius");
    camRightLoc_ = glGetUniformLocation(program_, "camRight");
    camUpLoc_    = glGetUniformLocation(program_, "camUp");
    colorLoc_    = glGetUniformLocation(program_, "color");
    lightDirLoc_ = glGetUniformLocation(program_, "lightDir");

    // The quad comes out of gl_VertexID, so there is no attribute to describe — but the
    // core profile still refuses to draw with no vertex array bound at all.
    glGenVertexArrays(1, &vao_);
}

SpherePipeline::~SpherePipeline()
{
    glDeleteVertexArrays(1, &vao_);
}

void SpherePipeline::draw(std::span<SceneObject const* const> objects, RenderView const& view)
{
    if (objects.empty()) {
        return;
    }

    // Opaque bodies, unlike everything else this renderer draws: depth decides what wins
    // rather than blend order. Set rather than assumed, since ImGui and the particle
    // pipelines each leave their own state behind.
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glUseProgram(program_);
    glUniformMatrix4fv(viewProjLoc_, 1, GL_FALSE, glm::value_ptr(view.viewProj));
    glUniform3fv(camRightLoc_, 1, glm::value_ptr(view.right));
    glUniform3fv(camUpLoc_, 1, glm::value_ptr(view.up));
    glUniform4fv(colorLoc_, 1, glm::value_ptr(color_));

    // The sliders can be dragged to all zeros, which normalize would turn into NaNs.
    float     length   = glm::length(lightDir_);
    glm::vec3 lightDir = (length > 1e-4F)? lightDir_ / length : glm::vec3 {0.0F, 1.0F, 0.0F};
    glUniform3fv(lightDirLoc_, 1, glm::value_ptr(lightDir));

    glBindVertexArray(vao_);

    for (SceneObject const* object : objects) {
        if (object->radius <= 0.0F) {
            continue; // a zero-radius body would collapse the quad to a point
        }

        glUniform2fv(centerLoc_, 1, glm::value_ptr(object->position));
        glUniform1f(radiusLoc_, object->radius);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindVertexArray(0);

    // The particle pipelines draw unsorted and expect no depth test, so hand the state
    // back the way they want it.
    glDisable(GL_DEPTH_TEST);
}

void SpherePipeline::renderSettings()
{
    ImGui::Begin("Objects");

    ImGui::ColorEdit3("Color", glm::value_ptr(color_));
    ImGui::SliderFloat3("Light direction", glm::value_ptr(lightDir_), -1.0F, 1.0F);
    ImGui::SetItemTooltip("World space, so the shading stays put as the camera orbits");

    ImGui::End();
}
