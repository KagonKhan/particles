#ifndef YARR_LOGIC_SHAPE_HPP
#define YARR_LOGIC_SHAPE_HPP

#include <glm/common.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <variant>


struct Circle { float radius {0.25F}; };

struct Box { glm::vec2 halfExtents {0.25F, 0.25F}; };

struct Segment { float halfLength {1.0F}; float thickness {0.05F}; };

struct HalfPlane { float drawExtent {8.0F}; };

struct Frame
{
    glm::vec2 halfExtents {4.0F, 3.0F}; // the opening, i.e. the usable world
    float thickness {0.2F};
};


using Shape = std::variant<Circle, Box, Segment, HalfPlane, Frame>;

enum class ObjectType : std::uint8_t
{
    Circle,
    Box,
    Segment,
    HalfPlane,
    Frame,
};

static_assert(std::is_same_v<std::variant_alternative_t<0, Shape>, ::Circle>);
static_assert(std::is_same_v<std::variant_alternative_t<1, Shape>, ::Box>);
static_assert(std::is_same_v<std::variant_alternative_t<2, Shape>, ::Segment>);
static_assert(std::is_same_v<std::variant_alternative_t<3, Shape>, ::HalfPlane>);
static_assert(std::is_same_v<std::variant_alternative_t<4, Shape>, ::Frame>);

inline constexpr std::array<char const*, std::variant_size_v<Shape>> kShapeNames {
    "Circle",
    "Box",
    "Segment",
    "Half plane",
    "Frame",
};

[[nodiscard]] inline ObjectType typeOf(Shape const& shape) noexcept
{
    return static_cast<ObjectType>(shape.index());
}

[[nodiscard]] inline char const* shapeName(Shape const& shape) noexcept { return kShapeNames[shape.index()]; }

[[nodiscard]] inline Shape defaultShape(ObjectType type) noexcept
{
    switch (type) {
    case ObjectType::Box:
        return Box {};

    case ObjectType::Segment:
        return Segment {};

    case ObjectType::HalfPlane:
        return HalfPlane {};

    case ObjectType::Frame:
        return Frame {};

    case ObjectType::Circle:
        return Circle {};
    }

    return Circle {};
}

// === DISTANCE ========================================================================================================
// Signed distance to the outline, negative inside, in the shape's own frame. Free
// overloads, so adding a shape stops the visits below compiling until it is handled.

[[nodiscard]] inline float signedDistance(Circle shape, glm::vec2 point) noexcept
{
    return glm::length(point) - shape.radius;
}

[[nodiscard]] inline float signedDistance(Box shape, glm::vec2 point) noexcept
{
    glm::vec2 const outside = glm::abs(point) - shape.halfExtents;
    return glm::length(glm::max(outside, 0.0F)) + std::min(std::max(outside.x, outside.y), 0.0F);
}

[[nodiscard]] inline float signedDistance(Segment shape, glm::vec2 point) noexcept
{
    point.x -= std::clamp(point.x, -shape.halfLength, shape.halfLength);
    return glm::length(point) - (shape.thickness * 0.5F);
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

[[nodiscard]] inline float signedDistance(Shape const& shape, glm::vec2 point) noexcept
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

[[nodiscard]] inline glm::vec2 gradient(Shape const& shape, glm::vec2 point) noexcept
{
    return std::visit([point] (auto concrete) { return gradient(concrete, point); }, shape);
}

// === FILLET ==========================================================================================================
// Largest ball that rolls along the inside of the outline without changing it, and so how
// much a body's edges may be rounded by. A box tolerates none — rounded corners are not
// the box. shape.frag mirrors these and has to stay in step.

[[nodiscard]] inline float filletRadius(Circle shape) noexcept                 { return shape.radius; }
[[nodiscard]] inline float filletRadius([[maybe_unused]] Box shape) noexcept   { return 0.0F; }
[[nodiscard]] inline float filletRadius(Segment shape) noexcept                { return shape.thickness * 0.5F; }
[[nodiscard]] inline float filletRadius([[maybe_unused]] HalfPlane s) noexcept { return 0.0F; }
[[nodiscard]] inline float filletRadius([[maybe_unused]] Frame shape) noexcept { return 0.0F; }

[[nodiscard]] inline float filletRadius(Shape const& shape) noexcept
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

[[nodiscard]] inline Contact contact(Shape const& shape, glm::vec2 point) noexcept
{
    return std::visit([point] (auto concrete) { return contact(concrete, point); }, shape);
}

#endif // YARR_LOGIC_SHAPE_HPP
