#include "renderer.hpp"

#include "app/scene.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

namespace
{

constexpr glm::vec3 kOrbitTarget {0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kWorldUp {0.0F, 1.0F, 0.0F};

// An orthographic eye position is arbitrary — zoom comes from the box size, not the
// standoff. Parking it well back keeps every particle in front of the camera plane
// so the depth term stays positive, and the far plane covers a long-lived cloud.
constexpr float kOrthoStandoff = 50.0F;
constexpr float kOrthoFar      = 500.0F;

} // namespace

glm::vec3 Camera::forward() const noexcept
{
    return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
}

glm::vec3 Camera::right() const noexcept
{
    // Safe while pitch stays inside +-89 degrees, which both the slider and the Top
    // button respect; at a true pole this cross product collapses to zero.
    return glm::normalize(glm::cross(forward(), kWorldUp));
}

glm::vec3 Camera::up() const noexcept
{
    return glm::cross(right(), forward());
}

glm::vec3 Camera::eye() const noexcept
{
    float standoff = (projection == Projection::Orthographic)? kOrthoStandoff : distance;
    return kOrbitTarget - (forward() * standoff);
}

glm::mat4 Camera::view() const noexcept
{
    return glm::lookAt(eye(), kOrbitTarget, kWorldUp);
}

float Camera::depthReference() const noexcept
{
    // Derived from eye() rather than repeating its projection test, so the two cannot
    // drift apart: this is just the depth of the orbit target itself.
    return glm::length(kOrbitTarget - eye());
}

glm::mat4 Camera::viewProj(float aspect) const noexcept
{
    if (projection == Projection::Orthographic) {
        // Size the box to the vertical extent the perspective view would show at the
        // target plane, so flipping between 2D and 3D doesn't jump the cloud's
        // on-screen scale and the same Distance/FOV sliders keep working in both.
        float halfHeight = distance * std::tan(glm::radians(fovDegrees) * 0.5F);
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
    };
}

void Renderer::renderSettings(float dt)
{
    ImGui::Begin("Renderer");
    ImGui::Text("FPS: %f", 1.0F / dt);
    ImGui::Text("DT: %f", dt);

    ImGui::SeparatorText("Mode");

    int mode = static_cast<int>(mode_);
    ImGui::RadioButton("Points", &mode, static_cast<int>(RenderMode::Points));
    ImGui::SetItemTooltip("One GL_POINTS draw. Simple, and what to use while working on the simulation.");
    ImGui::SameLine();
    ImGui::RadioButton("Splat", &mode, static_cast<int>(RenderMode::Splat));
    ImGui::SetItemTooltip("Compute-accumulated density. Survives counts the point rasterizer will not.");
    mode_ = static_cast<RenderMode>(mode);

    ImGui::End();

    ImGui::Begin("Camera");

    int  projection = static_cast<int>(camera_.projection);
    bool switched   = ImGui::RadioButton("3D", &projection, static_cast<int>(Projection::Perspective));
    ImGui::SameLine();
    switched = ImGui::RadioButton("2D", &projection, static_cast<int>(Projection::Orthographic)) || switched;
    ImGui::SetItemTooltip("Orthographic: same cloud, no perspective foreshortening");

    if (switched) {
        camera_.projection = static_cast<Projection>(projection);

        // Flattening the projection alone still looks 3D, because a volumetric burst
        // keeps moving toward and away from the camera. Emission follows the mode by
        // default, but only on the switch, so the override below stays overridable.
        planarEmission_ = (camera_.projection == Projection::Orthographic);
    }

    ImGui::Checkbox("Planar emission", &planarEmission_);
    ImGui::SetItemTooltip(
        "Confine new particles to the plane facing the camera.\n"
        "Existing particles keep the motion they were born with.");

    // Axis-aligned views, which is what makes the 2D mode read as a flat plan/elevation
    // rather than just an unforeshortened 3D shot. Top stops at the same 89 degrees the
    // pitch slider does, because a camera looking straight down the world up axis makes
    // lookAt degenerate.
    if (ImGui::Button("Top")) {
        camera_.yaw   = 0.0F;
        camera_.pitch = glm::radians(89.0F);
    }

    ImGui::SameLine();
    if (ImGui::Button("Front")) {
        camera_.yaw   = 0.0F;
        camera_.pitch = 0.0F;
    }

    ImGui::SameLine();
    if (ImGui::Button("Side")) {
        camera_.yaw   = glm::radians(90.0F);
        camera_.pitch = 0.0F;
    }

    ImGui::SliderAngle("Yaw", &camera_.yaw, -180.0F, 180.0F);
    ImGui::SliderAngle("Pitch", &camera_.pitch, -89.0F, 89.0F);
    ImGui::SliderFloat("Distance", &camera_.distance, 0.1F, 20.0F);
    ImGui::SetItemTooltip("In 2D this scales the view box rather than moving the eye");
    ImGui::SliderFloat("FOV", &camera_.fovDegrees, 10.0F, 120.0F, "%.0f deg");
    ImGui::Checkbox("Auto orbit", &autoOrbit_);
    ImGui::End();

    if (autoOrbit_) {
        camera_.yaw = std::fmod(camera_.yaw + (dt * 0.4F), 6.28318530718F);
    }
}

void Renderer::spawnFromMouse(Scene& scene, glm::mat4 const& viewProj, int w, int h, float dt)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return;
    }

    ImVec2               mouse    = ImGui::GetMousePos();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float                local_x  = mouse.x - viewport->Pos.x;
    float                local_y  = mouse.y - viewport->Pos.y;

    float ndc_x = ((2.0F * local_x) / (float)w) - 1.0F;
    float ndc_y = 1.0F - ((2.0F * local_y) / (float)h);

    // Cast a ray through the cursor and land it on the plane that passes through
    // the orbit target facing the camera, so clicks always spawn in view.
    glm::mat4 invViewProj = glm::inverse(viewProj);
    glm::vec4 nearPoint   = invViewProj * glm::vec4(ndc_x, ndc_y, -1.0F, 1.0F);
    glm::vec4 farPoint    = invViewProj * glm::vec4(ndc_x, ndc_y, 1.0F, 1.0F);
    nearPoint /= nearPoint.w;
    farPoint  /= farPoint.w;

    glm::vec3 origin  = glm::vec3(nearPoint);
    glm::vec3 ray     = glm::normalize(glm::vec3(farPoint - nearPoint));
    glm::vec3 forward = camera_.forward();

    float denom = glm::dot(ray, forward);
    if (std::abs(denom) <= 1e-6F) {
        return;
    }

    float t = glm::dot(kOrbitTarget - origin, forward) / denom;
    if (t > 0.0F) {
        scene.spawn(
            {.origin = origin + (ray * t),
             .right  = camera_.right(),
             .up     = camera_.up(),
             .planar = planarEmission_},
            dt);
    }
}

void Renderer::render(GLFWwindow* window, Scene& scene, float dt)
{
    renderSettings(dt);

    // Only the active pipeline's settings, so the inactive one's knobs are not sitting
    // there looking like they do something.
    if (mode_ == RenderMode::Points) {
        points_.renderSettings();
    }
    else {
        splat_.renderSettings();
    }

    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    float aspect = (h > 0)? (float)w / (float)h : 1.0F;

    RenderView view = camera_.renderView(aspect);

    spawnFromMouse(scene, view.viewProj, w, h, dt);

    // One upload, whichever pipeline consumes it, then one fence covering its draw.
    particles_.upload(scene.positions());

    if (mode_ == RenderMode::Points) {
        points_.draw(particles_, view);
    }
    else {
        splat_.draw(particles_, view, w, h);
    }

    particles_.fence();
}
