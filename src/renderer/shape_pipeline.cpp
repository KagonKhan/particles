#include "shape_pipeline.hpp"

#include "utils/shader_cache.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <cstddef>
#include <variant>

namespace
{

struct Drawn
{
    ObjectType type;
    glm::vec2  dimensions; // as shape.frag reads them back out
    float      wall;       // a third dimension for shapes that need one, frame only so far
    glm::vec2  offset;     // local-space shift from the object's origin
    glm::vec2  footprint;  // local half-extents about the shifted origin
};

[[nodiscard]] Drawn drawnAs(Circle shape) noexcept
{
    return {ObjectType::Circle, {shape.radius, 0.0F}, 0.0F, {}, glm::vec2 {shape.radius}};
}

[[nodiscard]] Drawn drawnAs(Box shape) noexcept
{
    return {ObjectType::Box, shape.halfExtents, 0.0F, {}, shape.halfExtents};
}

[[nodiscard]] Drawn drawnAs(Segment shape) noexcept
{
    float const half = shape.thickness * 0.5F;
    return {ObjectType::Segment, {shape.halfLength, half}, 0.0F, {}, {shape.halfLength + half, half}};
}

// Unbounded in the field the simulation sees, so it is drawn as the finite slab hanging
// off its own outward face. A bounding sphere has nothing to hold onto otherwise.
[[nodiscard]] Drawn drawnAs(HalfPlane shape) noexcept
{
    glm::vec2 const half {shape.drawExtent, shape.drawExtent * 0.5F};
    return {ObjectType::Box, half, 0.0F, {0.0F, -half.y}, half};
}

// Dimensions are the wall's centreline, matching how the distance field is built. The
// footprint has to reach the outer face or the bounding sphere clips the frame's corners.
[[nodiscard]] Drawn drawnAs(Frame shape) noexcept
{
    float const half = shape.thickness * 0.5F;
    return {ObjectType::Frame, shape.halfExtents + half, half, {}, shape.halfExtents + shape.thickness};
}

[[nodiscard]] void const* byteOffset(std::size_t offset) noexcept
{
    return reinterpret_cast<void const*>(offset);
}

} // namespace

ShapePipeline::ShapePipeline()
{
    program_ = ShaderCache::getProgram("ShapeProgram");

    viewProjLoc_      = glGetUniformLocation(program_, "viewProj");
    camRightLoc_      = glGetUniformLocation(program_, "camRight");
    camUpLoc_         = glGetUniformLocation(program_, "camUp");
    cameraEyeLoc_     = glGetUniformLocation(program_, "cameraEye");
    cameraForwardLoc_ = glGetUniformLocation(program_, "cameraForward");
    orthographicLoc_  = glGetUniformLocation(program_, "orthographic");
    lightDirLoc_      = glGetUniformLocation(program_, "lightDir");
    ambientLoc_       = glGetUniformLocation(program_, "ambient");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &buffer_);

    // The quad's corners come out of gl_VertexID, so every attribute here is per-instance.
    constexpr auto kStride = static_cast<GLsizei>(sizeof(GpuBody));

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);

    for (GLuint location = 0; location < 3; ++location) {
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, kStride, byteOffset(location * sizeof(glm::vec4)));
        glVertexAttribDivisor(location, 1);
    }

    glBindVertexArray(0);
}

ShapePipeline::~ShapePipeline()
{
    glDeleteBuffers(1, &buffer_);
    glDeleteVertexArrays(1, &vao_);
}

void ShapePipeline::draw(std::span<SceneObject const* const> objects, RenderView const& view)
{
    bodies_.clear();

    for (SceneObject const* object : objects) {
        if ((object == nullptr) || !object->visible) {
            continue;
        }

        if (object->height <= 0.0F) {
            continue; // no thickness, so the march has nothing to hit
        }

        Drawn const     drawn  = std::visit([] (auto concrete) { return drawnAs(concrete); }, object->shape);
        glm::vec2 const centre = object->transform.position + drawn.offset;
        float const     bounds = glm::length(glm::vec2 {glm::length(drawn.footprint), object->height});

        bodies_.push_back({
            .origin = {
                centre.x, centre.y,
                static_cast<float>(static_cast<std::uint8_t>(drawn.type)), drawn.wall
            },
            .params = {drawn.dimensions.x, drawn.dimensions.y, object->height, bounds},
            .color  = object->color,
        });
    }

    if (bodies_.empty()) {
        return;
    }

    // Opaque bodies, unlike everything else this renderer draws: depth decides what wins
    // rather than blend order. Set rather than assumed, since ImGui and the particle
    // pipelines each leave their own state behind.
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    auto const bytes = static_cast<GLsizeiptr>(bodies_.size() * sizeof(GpuBody));

    glBindBuffer(GL_ARRAY_BUFFER, buffer_);

    if (bytes > capacity_) {
        glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
        capacity_ = bytes;
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, bodies_.data());

    glUseProgram(program_);
    glUniformMatrix4fv(viewProjLoc_, 1, GL_FALSE, glm::value_ptr(view.viewProj));
    glUniform3fv(camRightLoc_, 1, glm::value_ptr(view.right));
    glUniform3fv(camUpLoc_, 1, glm::value_ptr(view.up));
    glUniform3fv(cameraEyeLoc_, 1, glm::value_ptr(view.eye));
    glUniform3fv(cameraForwardLoc_, 1, glm::value_ptr(view.forward));
    glUniform1i(orthographicLoc_, view.orthographic? 1 : 0);
    glUniform1f(ambientLoc_, ambient_);

    // The sliders can be dragged to all zeros, which normalize would turn into NaNs.
    float const     length   = glm::length(lightDir_);
    glm::vec3 const lightDir = (length > 1e-4F)? lightDir_ / length : glm::vec3 {0.0F, 1.0F, 0.0F};
    glUniform3fv(lightDirLoc_, 1, glm::value_ptr(lightDir));

    glBindVertexArray(vao_);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(bodies_.size()));
    glBindVertexArray(0);

    // The particle pipelines draw unsorted and expect no depth test, so hand the state
    // back the way they want it.
    glDisable(GL_DEPTH_TEST);
}

void ShapePipeline::renderSettings()
{
    ImGui::Begin("Objects");

    ImGui::Text("Bodies drawn: %zu", bodies_.size());

    ImGui::SliderFloat3("Light direction", glm::value_ptr(lightDir_), -1.0F, 1.0F);
    ImGui::SetItemTooltip("World space, so the shading stays put as the camera orbits");

    ImGui::SliderFloat("Ambient", &ambient_, 0.0F, 1.0F, "%.2f");

    ImGui::End();
}
