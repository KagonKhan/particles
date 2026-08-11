#include "renderer.hpp"

#include "app/scene.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <algorithm>
#include <numbers>
#include <vector>

namespace
{

constexpr float kMoveSpeed = 3.0F; // world units per second
constexpr float kTurnSpeed = 1.5F; // radians per second

constexpr float kLookSensitivity = 0.0035F;                    // radians per pixel of mouse travel
constexpr float kPitchLimit      = 1.55334F;                   // 89 degrees, just shy of vertical
constexpr float kTwoPi           = 2.0F * std::numbers::pi_v<float>;

} // namespace

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
    // rather than just an unforeshortened 3D shot. Each one also flies the camera back to
    // where the origin is in frame, since a free camera could be anywhere by then. Top
    // stops at 89 degrees, the same place the pitch slider does, because a camera looking
    // straight down the world up axis makes lookAt degenerate.
    auto snapView = [this] (float degreesYaw, float degreesPitch) {
            camera_.yaw      = glm::radians(degreesYaw);
            camera_.pitch    = glm::radians(degreesPitch);
            camera_.position = -camera_.forward() * camera_.focusDistance;
        };

    if (ImGui::Button("Top")) {
        snapView(0.0F, -89.0F);
    }

    ImGui::SameLine();
    if (ImGui::Button("Front")) {
        snapView(0.0F, 0.0F);
    }

    ImGui::SameLine();
    if (ImGui::Button("Side")) {
        snapView(90.0F, 0.0F);
    }

    ImGui::SliderAngle("Yaw", &camera_.yaw, -180.0F, 180.0F);
    ImGui::SliderAngle("Pitch", &camera_.pitch, -89.0F, 89.0F);
    ImGui::DragFloat3("Position", glm::value_ptr(camera_.position), 0.05F);
    ImGui::SliderFloat("Focus distance", &camera_.focusDistance, 0.1F, 20.0F);
    ImGui::SetItemTooltip(
        "How far ahead the plane of interest sits: sets the 2D zoom, the distance a\n"
        "particle draws at nominal size, and the depth a click spawns at");
    ImGui::SliderFloat("FOV", &camera_.fovDegrees, 10.0F, 120.0F, "%.0f deg");
    ImGui::Checkbox("Auto turn", &autoTurn_);
    ImGui::SetItemTooltip("Sweeps the view around where the camera stands");
    ImGui::TextDisabled("WASD flies, right-drag or Q/E looks, Shift for speed");
    ImGui::End();

    if (autoTurn_) {
        camera_.yaw += dt * 0.4F;
    }

    ImGuiIO& io = ImGui::GetIO();

    // ImGui already tracks input state for its own widgets, so polling it here buys camera
    // control without a GLFW callback of its own. Movement is along the camera's own axes,
    // which is what makes it fly rather than slide along the world grid.
    if (!io.WantCaptureKeyboard) {
        float sprint = ImGui::IsKeyDown(ImGuiMod_Shift)? 4.0F : 1.0F;
        float move   = kMoveSpeed * sprint * dt;
        float turn   = kTurnSpeed * dt;

        glm::vec3 forward = camera_.forward();
        glm::vec3 right   = camera_.right();

        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            camera_.position += forward * move;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            camera_.position -= forward * move;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            camera_.position += right * move;
        }

        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            camera_.position -= right * move;
        }

        // Yaw grows counter-clockwise seen from above, so turning right subtracts.
        if (ImGui::IsKeyDown(ImGuiKey_Q)) {
            camera_.yaw += turn;
        }

        if (ImGui::IsKeyDown(ImGuiKey_E)) {
            camera_.yaw -= turn;
        }
    }

    // Mouse look on a held right button. Latched on the press, so a drag that wanders over
    // a panel keeps steering instead of stopping dead — only where it starts matters.
    if (mouseLooking_ || (!io.WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        mouseLooking_ = ImGui::IsMouseDown(ImGuiMouseButton_Right);

        // Not scaled by dt: a mouse reports how far it moved, not how fast, so scaling it
        // by frame time would make the same flick turn further on a slow frame.
        camera_.yaw   -= io.MouseDelta.x * kLookSensitivity;
        camera_.pitch -= io.MouseDelta.y * kLookSensitivity;

        // Stops short of vertical for the same reason the pitch slider does: straight down
        // the world up axis makes lookAt degenerate.
        camera_.pitch = std::clamp(camera_.pitch, -kPitchLimit, kPitchLimit);
    }

    // Wrap once, after every source has had its say, so the slider stays in range however
    // far the camera has been spun. remainder lands in [-pi, pi]; fmod would not.
    camera_.yaw = std::remainder(camera_.yaw, kTwoPi);
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

    // Cast a ray through the cursor and land it on the plane that passes through the
    // focus point facing the camera, so clicks always spawn in view.
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

    float t = glm::dot(camera_.focusPoint() - origin, forward) / denom;
    if (t > 0.0F) {
        scene.spawn(dt);
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

    spheres_.renderSettings();

    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    float aspect = (h > 0)? (float)w / (float)h : 1.0F;

    RenderView view = camera_.renderView(aspect);

    // Bodies first: they are the only opaque thing here, so they lay down the depth the
    // particles are then blended over.
    std::vector<SceneObject const*> objects = scene.getSceneObjects();
    spheres_.draw(objects, view);

    scene.spawn(dt);

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
