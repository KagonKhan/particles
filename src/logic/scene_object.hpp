#ifndef YARR_LOGIC_SCENE_OBJECT_HPP
#define YARR_LOGIC_SCENE_OBJECT_HPP

#include "logic/sdf.hpp"
#include "logic/shape.hpp"

#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <variant>


struct Transform
{
    glm::vec2 position {0.0F, 0.0F};
};


struct SceneObject
{
    Transform transform;
    Shape shape {Circle {}};

    float height {0.25F};

    glm::vec4 color {0.35F, 0.45F, 0.75F, 1.0F};
    bool visible {true};
};


// The solid the shape describes: its outline extruded `height` either side of the plane, with
// the edges filleted by as much as the outline can take. The extrusion itself is the one
// sdf.inl gives the renderer, so this is only the outline fed into it.
[[nodiscard]] inline float bodyDistance(Shape const& shape, float height, glm::vec3 local) noexcept
{
    return glsl::sdfExtrude(signedDistance(shape, glm::vec2 {local}), local.z, height, filletRadius(shape));
}

// Particles are flat, so they meet a body at z = 0.
[[nodiscard]] inline float signedDistance(SceneObject const& object, glm::vec2 world) noexcept
{
    glm::vec2 const local = world - object.transform.position;
    return bodyDistance(object.shape, object.height, {local.x, local.y, 0.0F});
}

// === SETTINGS ========================================================================================================

inline bool renderShapeFields(Circle& shape)
{
    return ImGui::SliderFloat("Radius", &shape.radius, 0.01F, 5.0F, "%.3f u");
}

inline bool renderShapeFields(Box& shape)
{
    return ImGui::SliderFloat2("Half extents", glm::value_ptr(shape.halfExtents), 0.01F, 5.0F, "%.3f u");
}

inline bool renderShapeFields(Segment& shape)
{
    bool changed = ImGui::SliderFloat("Half length", &shape.halfLength, 0.01F, 10.0F, "%.3f u");
    changed = ImGui::SliderFloat("Thickness", &shape.thickness, 0.01F, 2.0F, "%.3f u") || changed;

    return changed;
}

inline bool renderShapeFields(HalfPlane& shape)
{
    bool changed = ImGui::SliderFloat("Draw extent", &shape.drawExtent, 1.0F, 50.0F, "%.1f u");
    ImGui::SetItemTooltip("The plane is unbounded — this is the slab drawn to stand in for it");

    return changed;
}

inline bool renderShapeFields(Frame& shape)
{
    bool changed = ImGui::SliderFloat2("Opening", glm::value_ptr(shape.halfExtents), 0.1F, 20.0F, "%.3f u");
    ImGui::SetItemTooltip("Half extents of the world the frame encloses, measured to its inner face");
    changed = ImGui::SliderFloat("Thickness", &shape.thickness, 0.01F, 2.0F, "%.3f u") || changed;

    return changed;
}

inline bool renderShapeSettings(Shape& shape)
{
    int  type    = static_cast<int>(shape.index());
    bool changed = ImGui::Combo("Type", &type, kShapeNames.data(), static_cast<int>(kShapeNames.size()));

    if (changed) {
        shape = defaultShape(static_cast<std::size_t>(type));
    }

    return std::visit([] (auto& concrete) { return renderShapeFields(concrete); }, shape) || changed;
}

inline bool renderSceneObjectSettings(SceneObject& object)
{
    // Two objects can end up in one window sharing every label below.
    ImGui::PushID(&object);

    bool changed = ImGui::Checkbox("Visible", &object.visible);

    changed = ImGui::SliderFloat2(
        "Position",
        glm::value_ptr(object.transform.position),
        -10.0F,
        10.0F,
        "%.3f u") || changed;

    changed = renderShapeSettings(object.shape) || changed;

    changed = ImGui::SliderFloat("Height", &object.height, 0.01F, 5.0F, "%.3f u") || changed;
    ImGui::SetItemTooltip("How far the body reaches out of its own plane. Match a circle's radius for a sphere.");

    changed = ImGui::ColorEdit3("Color", glm::value_ptr(object.color)) || changed;

    ImGui::PopID();

    return changed;
}

// === SERIALIZATION ===================================================================================================
// Absent keys and values of the wrong type leave the field alone, as the knobs do.

namespace detail
{

inline void readFloat(nlohmann::json const& in, char const* key, float& out)
{
    if (auto const entry = in.find(key); (entry != in.end()) && entry->is_number()) {
        out = entry->get<float>();
    }
}

inline void readVec2(nlohmann::json const& in, char const* key, glm::vec2& out)
{
    if (auto const entry = in.find(key); (entry != in.end()) && entry->is_array() && (entry->size() == 2)) {
        out = {(*entry)[0].get<float>(), (*entry)[1].get<float>()};
    }
}

inline void readVec4(nlohmann::json const& in, char const* key, glm::vec4& out)
{
    if (auto const entry = in.find(key); (entry != in.end()) && entry->is_array() && (entry->size() == 4)) {
        out = {
            (*entry)[0].get<float>(), (*entry)[1].get<float>(),
            (*entry)[2].get<float>(), (*entry)[3].get<float>()
        };
    }
}

} // namespace detail

[[nodiscard]] inline nlohmann::json serializeShapeFields(Circle const& shape)
{
    return {{"radius", shape.radius}};
}

[[nodiscard]] inline nlohmann::json serializeShapeFields(Box const& shape)
{
    return {{"halfExtents", {shape.halfExtents.x, shape.halfExtents.y}}};
}

[[nodiscard]] inline nlohmann::json serializeShapeFields(Segment const& shape)
{
    return {{"halfLength", shape.halfLength}, {"thickness", shape.thickness}};
}

[[nodiscard]] inline nlohmann::json serializeShapeFields(HalfPlane const& shape)
{
    return {{"drawExtent", shape.drawExtent}};
}

[[nodiscard]] inline nlohmann::json serializeShapeFields(Frame const& shape)
{
    return {{"halfExtents", {shape.halfExtents.x, shape.halfExtents.y}}, {"thickness", shape.thickness}};
}

inline void deserializeShapeFields(Circle& shape, nlohmann::json const& in)
{
    detail::readFloat(in, "radius", shape.radius);
}

inline void deserializeShapeFields(Box& shape, nlohmann::json const& in)
{
    detail::readVec2(in, "halfExtents", shape.halfExtents);
}

inline void deserializeShapeFields(Segment& shape, nlohmann::json const& in)
{
    detail::readFloat(in, "halfLength", shape.halfLength);
    detail::readFloat(in, "thickness", shape.thickness);
}

inline void deserializeShapeFields(HalfPlane& shape, nlohmann::json const& in)
{
    detail::readFloat(in, "drawExtent", shape.drawExtent);
}

inline void deserializeShapeFields(Frame& shape, nlohmann::json const& in)
{
    detail::readVec2(in, "halfExtents", shape.halfExtents);
    detail::readFloat(in, "thickness", shape.thickness);
}

[[nodiscard]] inline nlohmann::json serialize(Shape const& shape)
{
    nlohmann::json out = std::visit([] (auto const& concrete) { return serializeShapeFields(concrete); }, shape);
    out["type"] = shapeName(shape);

    return out;
}

// The type is read first, since it decides which alternative then reads its own fields
// out of the same object. An unknown type leaves the variant as it was.
inline void deserialize(Shape& shape, nlohmann::json const& in)
{
    if (!in.is_object()) {
        return;
    }

    if (auto const entry = in.find("type"); (entry != in.end()) && entry->is_string()) {
        std::string const name = entry->get<std::string>();

        for (std::size_t i = 0; i < kShapeNames.size(); ++i) {
            if (name == kShapeNames[i]) {
                shape = defaultShape(i);
                break;
            }
        }
    }

    std::visit([&in] (auto& concrete) { deserializeShapeFields(concrete, in); }, shape);
}

[[nodiscard]] inline nlohmann::json serialize(SceneObject const& object)
{
    return {
        {"position", {object.transform.position.x, object.transform.position.y}},
        {"shape", serialize(object.shape)},
        {"height", object.height},
        {"color", {object.color.r, object.color.g, object.color.b, object.color.a}},
        {"visible", object.visible},
    };
}

inline void deserialize(SceneObject& object, nlohmann::json const& in)
{
    if (!in.is_object()) {
        return;
    }

    detail::readVec2(in, "position", object.transform.position);
    detail::readFloat(in, "height", object.height);
    detail::readVec4(in, "color", object.color);

    if (auto const entry = in.find("visible"); (entry != in.end()) && entry->is_boolean()) {
        object.visible = entry->get<bool>();
    }

    if (auto const entry = in.find("shape"); (entry != in.end())) {
        deserialize(object.shape, *entry);
    }
}

#endif // YARR_LOGIC_SCENE_OBJECT_HPP
