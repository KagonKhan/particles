#include "camera.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace
{

constexpr glm::vec3 kWorldUp {0.0F, 1.0F, 0.0F};

// An orthographic eye position is arbitrary along the view axis — zoom comes from the box
// size, not the standoff. Backing the eye off keeps every particle in front of the camera
// plane so the depth term stays positive, and the far plane covers a long-lived cloud.
constexpr float kOrthoStandoff = 50.0F;
constexpr float kOrthoFar      = 500.0F;

} // namespace

glm::vec3 Camera::forward() const noexcept
{
    return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
}

glm::vec3 Camera::right() const noexcept
{
    return glm::normalize(glm::cross(forward(), kWorldUp));
}

glm::vec3 Camera::up() const noexcept
{
    return glm::cross(right(), forward());
}

glm::vec3 Camera::eye() const noexcept
{
    // Under perspective the eye is simply where the camera is. Orthographic backs it off
    // along the view axis, which no more moves the picture than sliding a projector back.
    if (projection == Projection::Orthographic) {
        return position - (forward() * kOrthoStandoff);
    }

    return position;
}

glm::vec3 Camera::focusPoint() const noexcept
{
    return position + (forward() * focusDistance);
}

glm::mat4 Camera::view() const noexcept
{
    glm::vec3 from = eye();
    return glm::lookAt(from, from + forward(), kWorldUp);
}

float Camera::depthReference() const noexcept
{
    // Derived from eye() rather than repeating its projection test, so the two cannot
    // drift apart: this is just the depth of the focus point itself.
    return glm::length(focusPoint() - eye());
}

glm::mat4 Camera::viewProj(float aspect) const noexcept
{
    if (projection == Projection::Orthographic) {
        // Size the box to the vertical extent the perspective view would show at the
        // focus plane, so flipping between 2D and 3D doesn't jump the cloud's on-screen
        // scale and the same Focus/FOV sliders keep working in both.
        float halfHeight = focusDistance * std::tan(glm::radians(fovDegrees) * 0.5F);
        float halfWidth  = halfHeight * aspect;

        glm::mat4 proj = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.01F, kOrthoFar);
        return proj * view();
    }

    glm::mat4 proj = glm::perspective(glm::radians(fovDegrees), aspect, 0.01F, 1000.0F);
    return proj * view();
}

RenderView Camera::renderView(float aspect) const noexcept
{
    glm::mat4 v = view();

    return {
        .viewProj       = viewProj(aspect),
        .viewRowZ       = {v[0][2], v[1][2], v[2][2], v[3][2]},
        .depthReference = depthReference(),
        .right          = right(),
        .up             = up(),
    };
}
