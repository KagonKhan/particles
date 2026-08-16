#include "shape_pipeline.hpp"

#include "utils/shader_cache.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace
{

// The primitives shape.frag can march, and the whole of the contract between this file and
// it: kCircle, kBox and kSegment there are these values here.
//
// Deliberately not `Shape`'s list. A shape the shader has no case for is lowered on this side
// instead of being added on that one — a half plane to the slab it draws as, a frame to the
// four boxes its walls are — so this set stays at three however many shapes exist, and adding
// one cannot silently renumber what the shader is being told.
enum class GpuPrimitive : std::uint8_t
{
    Circle  = 0,
    Box     = 1,
    Segment = 2,
};

struct Drawn
{
    GpuPrimitive type;
    glm::vec2    dimensions; // as shape.frag reads them back out
    glm::vec2    offset;     // local-space shift from the object's origin
    glm::vec2    footprint;  // local half-extents about the shifted origin
};

// A shape is drawn as one body or as several, and the footprints are what the march is
// billed for: the vertex shader rasterizes each one's box and nothing outside it.
struct Drawing
{
    std::array<Drawn, 4> parts {};
    std::size_t          count {0};

    [[nodiscard]] auto begin() const noexcept { return parts.begin(); }
    [[nodiscard]] auto end() const noexcept   { return parts.begin() + static_cast<std::ptrdiff_t>(count); }
};

[[nodiscard]] Drawing drawnAs(Circle shape) noexcept
{
    return {{Drawn {GpuPrimitive::Circle, {shape.radius, 0.0F}, {}, glm::vec2 {shape.radius}}}, 1};
}

[[nodiscard]] Drawing drawnAs(Box shape) noexcept
{
    return {{Drawn {GpuPrimitive::Box, shape.halfExtents, {}, shape.halfExtents}}, 1};
}

[[nodiscard]] Drawing drawnAs(Segment shape) noexcept
{
    float const half = shape.thickness * 0.5F;
    return {{Drawn {GpuPrimitive::Segment, {shape.halfLength, half}, {}, {shape.halfLength + half, half}}}, 1};
}

// Unbounded in the field the simulation sees, so it is drawn as the finite slab hanging
// off its own outward face. A footprint has nothing to hold onto otherwise.
[[nodiscard]] Drawing drawnAs(HalfPlane shape) noexcept
{
    glm::vec2 const half {shape.drawExtent, shape.drawExtent * 0.5F};
    return {{Drawn {GpuPrimitive::Box, half, {0.0F, -half.y}, half}}, 1};
}

// Four walls rather than the one shelled box the distance field is: as a single body its
// footprint is the whole room, so every fragment inside the frame marches the field for
// thirty-two steps only to find the emptiness it was always going to find. The walls'
// footprints are the walls, which is the frame's outline and nothing else. Their union is
// the same solid — the caps run the full width and the sides fill in between them.
[[nodiscard]] Drawing drawnAs(Frame shape) noexcept
{
    float const     half = shape.thickness * 0.5F;
    glm::vec2 const side {half, shape.halfExtents.y};
    glm::vec2 const cap {shape.halfExtents.x + shape.thickness, half};

    float const sideX = shape.halfExtents.x + half;
    float const capY  = shape.halfExtents.y + half;

    return {
        {
            Drawn {GpuPrimitive::Box, side, {-sideX, 0.0F}, side},
            Drawn {GpuPrimitive::Box, side, {sideX, 0.0F}, side},
            Drawn {GpuPrimitive::Box, cap, {0.0F, -capY}, cap},
            Drawn {GpuPrimitive::Box, cap, {0.0F, capY}, cap},
        },
        4
    };
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

        Drawing const drawing = std::visit([] (auto concrete) { return drawnAs(concrete); }, object->shape);

        for (Drawn const& drawn : drawing) {
            glm::vec2 const centre = object->transform.position + drawn.offset;

            bodies_.push_back({
                .placement = {centre.x, centre.y, drawn.footprint.x, drawn.footprint.y},
                .params    = {
                    drawn.dimensions.x, drawn.dimensions.y, object->height,
                    static_cast<float>(static_cast<std::uint8_t>(drawn.type))
                },
                .color = object->color,
            });
        }
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
