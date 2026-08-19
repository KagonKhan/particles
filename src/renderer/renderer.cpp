#include "renderer.hpp"

#include "logic/scene.hpp"
#include "logic/simulation.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <algorithm>
#include <numbers>
#include <vector>

namespace
{

constexpr float MOVE_SPEED = 3.0F; // world units per second
constexpr float TURN_SPEED = 1.5F; // radians per second

constexpr float LOOK_SENSITIVITY = 0.0035F;                    // radians per pixel of mouse travel
constexpr float PITCH_LIMIT      = 1.55334F;                   // 89 degrees, just shy of vertical
constexpr float TWO_PI           = 2.0F * std::numbers::pi_v<float>;

} // namespace

void Renderer::renderCameraPanel()
{
    ImGui::SeparatorText("Camera");

    int  projection = static_cast<int>(camera_.projection);
    bool switched   = ImGui::RadioButton("3D", &projection, static_cast<int>(Projection::PERSPECTIVE));
    ImGui::SameLine();
    switched = ImGui::RadioButton("2D", &projection, static_cast<int>(Projection::ORTHOGRAPHIC)) || switched;
    ImGui::SetItemTooltip("Orthographic: same cloud, no perspective foreshortening");

    if (switched) {
        camera_.projection = static_cast<Projection>(projection);
    }

    // Axis-aligned views. Front is the one that matters: it looks square on at the plane
    // the simulation runs in, which is the view everything else is a departure from. Each
    // one also flies the camera back to where the origin is in frame, since a free camera
    // could be anywhere by then. Top stops at 89 degrees, the same place the pitch slider
    // does, because a camera looking straight down the world up axis makes lookAt
    // degenerate.
    auto snap_view = [this] (float degrees_yaw, float degrees_pitch) {
            camera_.yaw      = glm::radians(degrees_yaw);
            camera_.pitch    = glm::radians(degrees_pitch);
            camera_.position = -camera_.forward() * camera_.focusDistance;
        };

    if (ImGui::Button("Top")) {
        // Same yaw as Front, so pitching back up from here returns to Front rather than to
        // the mirrored view behind the plane.
        snap_view(180.0F, -89.0F);
    }

    ImGui::SetItemTooltip("Edge-on to the simulation plane, so the cloud collapses to a line");

    ImGui::SameLine();
    if (ImGui::Button("Front")) {
        // 180, not 0: both look square on at the plane, but from -z it is mirrored, so
        // only this one puts world +x on the right. snap_view derives the position from
        // the facing, so this also lands the camera on the correct side.
        snap_view(180.0F, 0.0F);
    }

    ImGui::SetItemTooltip("Square on to the simulation plane, +x right — the view the simulation is written for");

    ImGui::SameLine();
    if (ImGui::Button("Side")) {
        snap_view(90.0F, 0.0F);
    }

    ImGui::SetItemTooltip("Edge-on to the simulation plane, so the cloud collapses to a line");

    ImGui::SliderAngle("Yaw", &camera_.yaw, -180.0F, 180.0F);
    ImGui::SliderAngle("Pitch", &camera_.pitch, -89.0F, 89.0F);
    ImGui::DragFloat3("Position", glm::value_ptr(camera_.position), 0.05F);
    ImGui::SliderFloat("Focus distance", &camera_.focusDistance, 0.1F, 20.0F);
    ImGui::SetItemTooltip(
        "How far ahead the plane of interest sits: sets the 2D zoom and the distance a\n"
        "particle draws at nominal size");
    ImGui::SliderFloat("FOV", &camera_.fovDegrees, 10.0F, 120.0F, "%.0f deg");
    ImGui::Checkbox("Auto turn", &autoTurn_);
    ImGui::SetItemTooltip("Sweeps the view around where the camera stands");
    ImGui::TextDisabled("WASD flies, right-drag or Q/E looks, Shift for speed");
    ImGui::TextDisabled("Left-drag moves the emitter across the plane");
}

void Renderer::renderSettings(float dt)
{
    ImGui::SeparatorText("Mode");

    int mode = static_cast<int>(mode_);
    ImGui::RadioButton("Points", &mode, static_cast<int>(RenderMode::POINTS));
    ImGui::SetItemTooltip("One GL_POINTS draw. Simple, and what to use while working on the simulation.");
    ImGui::SameLine();
    ImGui::RadioButton("Splat", &mode, static_cast<int>(RenderMode::SPLAT));
    ImGui::SetItemTooltip("Compute-accumulated density. Survives counts the point rasterizer will not.");
    mode_ = static_cast<RenderMode>(mode);


    if (autoTurn_) {
        camera_.yaw += dt * 0.4F;
    }

    ImGuiIO& io = ImGui::GetIO();

    // ImGui already tracks input state for its own widgets, so polling it here buys camera
    // control without a GLFW callback of its own. Movement is along the camera's own axes,
    // which is what makes it fly rather than slide along the world grid.
    if (!io.WantCaptureKeyboard) {
        float sprint = ImGui::IsKeyDown(ImGuiMod_Shift)? 4.0F : 1.0F;
        float move   = MOVE_SPEED * sprint * dt;
        float turn   = TURN_SPEED * dt;

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
        camera_.yaw   -= io.MouseDelta.x * LOOK_SENSITIVITY;
        camera_.pitch -= io.MouseDelta.y * LOOK_SENSITIVITY;

        // Stops short of vertical for the same reason the pitch slider does: straight down
        // the world up axis makes lookAt degenerate.
        camera_.pitch = std::clamp(camera_.pitch, -PITCH_LIMIT, PITCH_LIMIT);
    }

    // Wrap once, after every source has had its say, so the slider stays in range however
    // far the camera has been spun. remainder lands in [-pi, pi]; fmod would not.
    camera_.yaw = std::remainder(camera_.yaw, TWO_PI);
}

void Renderer::dragEmitter(Scene& scene, glm::mat4 const& view_proj, int w, int h)
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

    // Cast a ray through the cursor and land it on the simulation plane. Now that the
    // world is flat there is a single unambiguous answer to "what is under the cursor",
    // which is what makes dragging the emitter around by hand possible at all.
    glm::mat4 inv_view_proj = glm::inverse(view_proj);
    glm::vec4 near_point    = inv_view_proj * glm::vec4(ndc_x, ndc_y, -1.0F, 1.0F);
    glm::vec4 far_point     = inv_view_proj * glm::vec4(ndc_x, ndc_y, 1.0F, 1.0F);
    near_point /= near_point.w;
    far_point  /= far_point.w;

    glm::vec3 origin = glm::vec3(near_point);
    glm::vec3 ray    = glm::vec3(far_point) - origin;

    // A camera looking along the plane rather than at it sees no point under the cursor at
    // all — the ray runs parallel and never crosses z = 0. Snap to Front to get it back.
    if (std::abs(ray.z) <= 1e-6F) {
        return;
    }

    float t = -origin.z / ray.z;
    if ((t < 0.0F) || (t > 1.0F)) {
        return; // the crossing is behind the eye or past the far plane
    }

    glm::vec3 hit = origin + (t * ray);
    scene.placeEmitter({hit.x, hit.y});
}

void Renderer::render(FramebufferSize size, Simulation& simulation, float dt)
{
    if (!showRenderer_.get()) {
        return;
    }

    ImGui::Begin("Renderer");
    renderSettings(dt);

    ImGui::SeparatorText("Particle rendering");
    // Only the active pipeline's settings, so the inactive one's knobs are not sitting
    // there looking like they do something.
    if (mode_ == RenderMode::POINTS) {
        points_.renderSettings();
    }
    else {
        splat_.renderSettings();
    }

    shapes_.renderSettings();
    renderCameraPanel();
    ImGui::End();


    float aspect = (size.height > 0)? (float)size.width / (float)size.height : 1.0F;

    RenderView view = camera_.renderView(aspect);

    // Outside the borrow below: this is the part of streaming that can block on the GPU, and
    // the simulation thread is not going to wait on a frame that has not finished drawing.
    particles_.acquire();

    // The one window this frame needs the simulation itself, kept to the three things that
    // cannot be done without it. Everything after is this frame's own copy.
    std::vector<SceneObject> objects;

    {
        auto scene = simulation.borrow();

        // Before the copy, so a body dragged across the screen is drawn under the cursor on
        // the same frame rather than trailing it by one.
        dragEmitter(*scene, view.viewProj, size.width, size.height);

        objects = scene->getSceneObjects();

        // One upload, whichever pipeline consumes it, then one fence covering its draw.
        particles_.upload(scene->positions());
    }

    // Bodies first: they are the only opaque thing here, so they lay down the depth the
    // particles are then blended over.
    shapes_.draw(objects, view);

    if (mode_ == RenderMode::POINTS) {
        points_.draw(particles_, view);
    }
    else {
        splat_.draw(particles_, view, size.width, size.height);
    }

    particles_.fence();
}
