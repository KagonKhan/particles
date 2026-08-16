#ifndef YARR_LOGIC_SHAPE_HPP
#define YARR_LOGIC_SHAPE_HPP

#include "logic/sdf.hpp"

#include <glm/common.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>


// A shape carries the name it goes by, so that the list of shapes is the variant below and
// nothing else. An enumeration and a table of names beside it would each be the same list
// written again, and a list written twice is a list that will disagree with itself.

struct Circle
{
    static constexpr char const* NAME = "Circle";

    float radius {0.25F};
};

struct Box
{
    static constexpr char const* NAME = "Box";

    glm::vec2 halfExtents {0.25F, 0.25F};
};

struct Segment
{
    static constexpr char const* NAME = "Segment";

    float halfLength {1.0F};
    float thickness {0.05F};
};

struct HalfPlane
{
    static constexpr char const* NAME = "Half plane";

    float drawExtent {8.0F};
};

struct Frame
{
    static constexpr char const* NAME = "Frame";

    glm::vec2 halfExtents {4.0F, 3.0F}; // the opening, i.e. the usable world
    float thickness {0.2F};
};


using Shape = std::variant<Circle, Box, Segment, HalfPlane, Frame>;

inline constexpr std::size_t SHAPE_COUNT = std::variant_size_v<Shape>;

namespace detail
{

template <std::size_t... I>
[[nodiscard]] constexpr auto shapeNames(std::index_sequence<I...> /*alternatives*/)
{
    return std::array<char const*, sizeof...(I)> {std::variant_alternative_t<I, Shape>::NAME...};
}

// The fold visits every alternative and exactly one of them assigns, which is a `switch` over
// the variant that cannot be written with a case missing.
template <std::size_t... I>
[[nodiscard]] Shape shapeOfIndex(std::size_t index, std::index_sequence<I...> /*alternatives*/) noexcept
{
    Shape out;
    ((I == index? void(out = std::variant_alternative_t<I, Shape> {}) : void()), ...);

    return out;
}

} // namespace detail

inline constexpr auto SHAPE_NAMES = detail::shapeNames(std::make_index_sequence<SHAPE_COUNT> {});

[[nodiscard]] inline char const* shapeName(Shape const& shape) noexcept { return SHAPE_NAMES[shape.index()]; }

// A default-constructed alternative by its index in the variant, which is what the type combo
// and the deserializer each have in hand. An index out of range gives the first alternative.
[[nodiscard]] inline Shape defaultShape(std::size_t index) noexcept
{
    return detail::shapeOfIndex(index, std::make_index_sequence<SHAPE_COUNT> {});
}

// `Shape` itself and nothing else, which every overload below that takes the variant is
// constrained on.
//
// Taking `Shape const&` plainly does not mean that: a variant is implicitly constructible
// from any one of its alternatives, so such an overload also accepts a single shape. It is
// then the candidate that a shape added *without* its own overload silently binds to — and
// since these all dispatch by visiting, that shape's case would call the dispatcher, which
// would visit, which would call the dispatcher, until the stack ran out. A shape missing its
// overload has to fail to compile; unconstrained it instead compiles and then dies at the
// first particle that reaches it.
template <typename T>
concept AnyShape = std::same_as<std::remove_cvref_t<T>, Shape>;

// === DISTANCE ========================================================================================================
// Signed distance to the outline, negative inside, in the shape's own frame. Free overloads,
// so adding a shape stops the visits below compiling until it is handled — which is what the
// AnyShape constraint on those visits is there to make true rather than nearly true.
//
// The three the renderer also marches read their field out of sdf.inl rather than stating it
// again. What is left here is the mapping from a shape's own parameters onto that field,
// which is the part the shader has no use for — it is handed dimensions already reduced.

[[nodiscard]] inline float signedDistance(Circle shape, glm::vec2 point) noexcept
{
    return glsl::sdfCircle(point, shape.radius);
}

[[nodiscard]] inline float signedDistance(Box shape, glm::vec2 point) noexcept
{
    return glsl::sdfBox(point, shape.halfExtents);
}

[[nodiscard]] inline float signedDistance(Segment shape, glm::vec2 point) noexcept
{
    return glsl::sdfSegment(point, shape.halfLength, shape.thickness * 0.5F);
}

[[nodiscard]] inline float signedDistance([[maybe_unused]] HalfPlane shape, glm::vec2 point) noexcept
{
    return point.y;
}

// The box's field shelled: abs turns a solid into a wall straddling its outline. Growing
// the extents by half the wall first puts its inner face on halfExtents, so the field
// reads as the opening rather than something to subtract by hand.
[[nodiscard]] inline float signedDistance(Frame shape, glm::vec2 point) noexcept
{
    float const half = shape.thickness * 0.5F;
    return std::abs(signedDistance(Box {shape.halfExtents + half}, point)) - half;
}

[[nodiscard]] inline float signedDistance(AnyShape auto const& shape, glm::vec2 point) noexcept
{
    return std::visit([point] (auto concrete) { return signedDistance(concrete, point); }, shape);
}

// === GRADIENT ========================================================================================================
// Unit outward direction of the distance field. Written out per shape rather than
// finite-differenced because this runs once per particle per object.
//
// All of them are undefined on the shape's medial axis, where the nearest point on the
// outline stops being unique, and return +y there rather than a NaN.

[[nodiscard]] inline glm::vec2 gradient([[maybe_unused]] Circle shape, glm::vec2 point) noexcept
{
    float const distance = glm::length(point);
    return (distance > 1e-6F)? point / distance : glm::vec2 {0.0F, 1.0F};
}

[[nodiscard]] inline glm::vec2 gradient(Box shape, glm::vec2 point) noexcept
{
    // Not glm::sign, which is zero on an axis: an interior point sitting on one, whose
    // nearest face is the one that axis picks out, would come back with no direction at all.
    glm::vec2 const quadrant {(point.x < 0.0F)? -1.0F : 1.0F, (point.y < 0.0F)? -1.0F : 1.0F};
    glm::vec2 const outside = glm::abs(point) - shape.halfExtents;

    if (std::max(outside.x, outside.y) > 0.0F) {
        return quadrant * glm::normalize(glm::max(outside, 0.0F));
    }

    return (outside.x > outside.y)? glm::vec2 {quadrant.x, 0.0F} : glm::vec2 {0.0F, quadrant.y};
}

[[nodiscard]] inline glm::vec2 gradient(Segment shape, glm::vec2 point) noexcept
{
    point.x -= std::clamp(point.x, -shape.halfLength, shape.halfLength);

    float const distance = glm::length(point);
    return (distance > 1e-6F)? point / distance : glm::vec2 {0.0F, 1.0F};
}

[[nodiscard]] inline glm::vec2 gradient([[maybe_unused]] HalfPlane shape,
    [[maybe_unused]] glm::vec2                                     point) noexcept
{
    return {0.0F, 1.0F};
}

// The wall's two faces point opposite ways, which is the abs in the distance differentiated.
[[nodiscard]] inline glm::vec2 gradient(Frame shape, glm::vec2 point) noexcept
{
    Box const   centreline {shape.halfExtents + (shape.thickness * 0.5F)};
    float const inward = (signedDistance(centreline, point) < 0.0F)? -1.0F : 1.0F;

    return inward * gradient(centreline, point);
}

[[nodiscard]] inline glm::vec2 gradient(AnyShape auto const& shape, glm::vec2 point) noexcept
{
    return std::visit([point] (auto concrete) { return gradient(concrete, point); }, shape);
}

// === FILLET ==========================================================================================================
// Largest ball that rolls along the inside of the outline without changing it, and so how
// much a body's edges may be rounded by. A box tolerates none — rounded corners are not
// the box. Out of sdf.inl, since the renderer rounds by this too and a body drawn with a
// different fillet than it collides with is a thing you can look straight at and not see.
//
// A half plane and a frame have no fillet of their own to share: neither is a primitive the
// shader knows, and shape_pipeline.cpp lowers both to boxes. So both take the box's, which
// is what they are in fact drawn with.

[[nodiscard]] inline float filletRadius(Circle shape) noexcept  { return glsl::sdfCircleFillet(shape.radius); }
[[nodiscard]] inline float filletRadius(Segment shape) noexcept { return glsl::sdfSegmentFillet(shape.thickness * 0.5F); }
[[nodiscard]] inline float filletRadius([[maybe_unused]] Box shape) noexcept       { return glsl::sdfBoxFillet(); }
[[nodiscard]] inline float filletRadius([[maybe_unused]] HalfPlane shape) noexcept { return glsl::sdfBoxFillet(); }
[[nodiscard]] inline float filletRadius([[maybe_unused]] Frame shape) noexcept     { return glsl::sdfBoxFillet(); }

[[nodiscard]] inline float filletRadius(AnyShape auto const& shape) noexcept
{
    return std::visit([] (auto concrete) { return filletRadius(concrete); }, shape);
}

// === CONTACT =========================================================================================================
// Where a particle may not be, and the way out of it. For a solid that is the field itself,
// negative inside, with the gradient pointing out.
//
// A frame is not a solid. Its field is a shell, so the room and the world beyond the wall
// are both positive, and resolving against it directly fails twice over: a particle that
// crosses the wall's mid-line in one step is pushed on through rather than back, and one
// that clears the wall outright is never in contact again and is gone. So a frame is
// resolved against the complement of its opening instead — the same surface, since the
// opening is the wall's inner face, but as a containment test that no step size can outrun.

struct Contact
{
    float     distance; // negative while the particle is somewhere it may not be
    glm::vec2 normal;   // unit, pointing at where it may
};

[[nodiscard]] inline Contact contact(Circle shape, glm::vec2 point) noexcept
{
    return {.distance = signedDistance(shape, point), .normal = gradient(shape, point)};
}

[[nodiscard]] inline Contact contact(Box shape, glm::vec2 point) noexcept
{
    return {.distance = signedDistance(shape, point), .normal = gradient(shape, point)};
}

[[nodiscard]] inline Contact contact(Segment shape, glm::vec2 point) noexcept
{
    return {.distance = signedDistance(shape, point), .normal = gradient(shape, point)};
}

[[nodiscard]] inline Contact contact(HalfPlane shape, glm::vec2 point) noexcept
{
    return {.distance = signedDistance(shape, point), .normal = gradient(shape, point)};
}

[[nodiscard]] inline Contact contact(Frame shape, glm::vec2 point) noexcept
{
    Box const opening {shape.halfExtents};
    return {.distance = -signedDistance(opening, point), .normal = -gradient(opening, point)};
}

[[nodiscard]] inline Contact contact(AnyShape auto const& shape, glm::vec2 point) noexcept
{
    return std::visit([point] (auto concrete) { return contact(concrete, point); }, shape);
}

// === BROAD PHASE =====================================================================================================
// Whether a point is close enough to be worth the full contact test: false only when its
// contact distance is certainly at least `margin`. Conservative in that one direction, so a
// caller may skip on false and nothing else changes.
//
// All of them stop at the box-ish underestimate of the field — no square root, no gradient
// — which is the whole point: almost every particle in a pool is nowhere near any one body,
// and this is all such a particle is allowed to cost.
//
// No Shape overload on purpose. Reached through the variant this would cost more than the
// test it saves; it is meant to be called with the alternative already in hand.

[[nodiscard]] inline bool mayContact(Circle shape, glm::vec2 point, float margin) noexcept
{
    float const reach = shape.radius + margin;
    return glm::dot(point, point) < (reach * reach);
}

[[nodiscard]] inline bool mayContact(Box shape, glm::vec2 point, float margin) noexcept
{
    glm::vec2 const outside = glm::abs(point) - shape.halfExtents;
    return std::max(outside.x, outside.y) < margin;
}

// The capsule sits inside this box, so the box's distance never overstates the capsule's.
[[nodiscard]] inline bool mayContact(Segment shape, glm::vec2 point, float margin) noexcept
{
    float const half = shape.thickness * 0.5F;
    return mayContact(Box {{shape.halfLength + half, half}}, point, margin);
}

[[nodiscard]] inline bool mayContact([[maybe_unused]] HalfPlane shape, glm::vec2 point, float margin) noexcept
{
    return point.y < margin;
}

// Inverted along with the contact itself: a frame is escaped by leaving its opening, so the
// particles worth testing are the ones the opening does not comfortably contain.
[[nodiscard]] inline bool mayContact(Frame shape, glm::vec2 point, float margin) noexcept
{
    glm::vec2 const outside = glm::abs(point) - shape.halfExtents;
    return std::max(outside.x, outside.y) > -margin;
}

#endif // YARR_LOGIC_SHAPE_HPP
