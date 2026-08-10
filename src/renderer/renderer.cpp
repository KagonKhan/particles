#include "renderer.hpp"

#include "app/scene.hpp"
#include "emitter/emitter.hpp"
#include "utils/opengl.hpp"
#include "utils/shader_cache.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace
{

constexpr std::size_t BufferCount {3};

GLuint      splatProgram;   // compute: particle positions -> density image
GLuint      resolveProgram; // fullscreen: density image -> color
GLuint      fullscreenVAO;  // empty VAO, fullscreen triangle uses gl_VertexID
GLuint      densityTexture;
GLuint      particleSSBO[BufferCount];
void*       mappedPtr[BufferCount];
GLsync      fences[BufferCount] = {};
std::size_t currentBuffer       = 0;

constexpr GLbitfield kStorageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

void* createMappedStorage(GLuint buffer, GLsizeiptr bytes)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, kStorageFlags);

    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bytes, kStorageFlags);
    if (ptr == nullptr) {
        throw std::runtime_error("Failed to map particle buffer");
    }

    return ptr;
}

GLuint createUintTexture(int w, int h)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

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

Renderer::~Renderer()
{
    for (std::size_t i = 0; i < BufferCount; ++i) {
        if (fences[i]) {
            glDeleteSync(fences[i]);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO[i]);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    glDeleteBuffers(BufferCount, particleSSBO);
    glDeleteTextures(1, &densityTexture);
    glDeleteVertexArrays(1, &fullscreenVAO);
}

Renderer::Renderer()
{
    splatProgram   = ShaderCache::getProgram("SplatProgram");
    resolveProgram = ShaderCache::getProgram("ResolveProgram");

    particleCountLoc_ = glGetUniformLocation(splatProgram, "particleCount");
    screenSizeLoc_    = glGetUniformLocation(splatProgram, "screenSize");
    viewProjLoc_      = glGetUniformLocation(splatProgram, "viewProj");

    depthFalloffLoc_   = glGetUniformLocation(splatProgram, "depthFalloff");
    depthReferenceLoc_ = glGetUniformLocation(splatProgram, "depthReference");
    viewRowZLoc_       = glGetUniformLocation(splatProgram, "viewRowZ");
    particleRadiusLoc_ = glGetUniformLocation(splatProgram, "particleRadius");

    colorLoc_          = glGetUniformLocation(resolveProgram, "particleColor");
    densitySamplerLoc_ = glGetUniformLocation(resolveProgram, "densityImage");
    fadeLoc_           = glGetUniformLocation(resolveProgram, "fadeScale");

    std::array<GLint, 3> maxGroups {};
    for (GLuint dim = 0; dim < maxGroups.size(); ++dim) {
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, dim, &maxGroups[dim]);
    }

    maxWorkGroups_ = static_cast<GLuint>(maxGroups[0]);
    spdlog::info(
        "max compute work groups: {} x {} x {} ({} particles per dispatch axis)",
        maxGroups[0],
        maxGroups[1],
        maxGroups[2],
        static_cast<std::uint64_t>(maxWorkGroups_) * 256U);

    // Fullscreen triangle needs no vertex data at all — gl_VertexID drives it.
    glGenVertexArrays(1, &fullscreenVAO);

    // --- Particle storage buffers, persistent-mapped, used as SSBOs now ---
    glGenBuffers(BufferCount, particleSSBO);

    for (std::size_t i = 0; i < BufferCount; ++i) {
        mappedPtr[i] = createMappedStorage(particleSSBO[i], MAX_PARTICLES * sizeof(ParticleVector));
    }

    texW_ = 0; // force creation on first frame
    texH_ = 0;
}

void Renderer::resizeDensityTexture(int w, int h)
{
    if ((w == texW_) && (h == texH_)) {
        return;
    }

    texW_ = w;
    texH_ = h;

    if (densityTexture != 0) {
        glDeleteTextures(1, &densityTexture);
    }

    densityTexture = createUintTexture(w, h);
}

void Renderer::renderSettings(float dt)
{
    ImGui::Begin("Renderer");
    ImGui::Text("FPS: %f", 1.0F / dt);
    ImGui::Text("DT: %f", dt);
    ImGui::SliderFloat("Density fade", &fadeScale_, 0.01F, 1.0F);
    ImGui::ColorEdit4("Particle color", glm::value_ptr(particleColor_));

    ImGui::SliderInt("Particle size", &particleRadius_, 0, 8, "%d px radius");
    ImGui::SetItemTooltip(
        "0 = one pixel per particle.\n"
        "Cost is quadratic: every pixel of the disc is an atomic add,\n"
        "so radius 4 is ~50x the splat work of radius 0.");

    ImGui::SliderFloat("Depth falloff", &depthFalloff_, 0.0F, 3.0F);
    ImGui::SetItemTooltip(
        "0 = flat, 1 = 1/depth, 2 = inverse-square.\n"
        "Near-flat in 2D: a parallel projection has no distance attenuation.");
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

    int w = 0;
    int h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    float     aspect   = (h > 0)? (float)w / (float)h : 1.0F;
    glm::mat4 viewProj = camera_.viewProj(aspect);

    spawnFromMouse(scene, viewProj, w, h, dt);

    resizeDensityTexture(w, h);

    currentBuffer = (currentBuffer + 1) % BufferCount;
    if (fences[currentBuffer]) {
        GLenum result = glClientWaitSync(fences[currentBuffer], GL_SYNC_FLUSH_COMMANDS_BIT, 1'000'000'000);
        if ((result == GL_TIMEOUT_EXPIRED) || (result == GL_WAIT_FAILED)) {
            spdlog::warn("particle buffer fence wait failed/timed out");
        }

        glDeleteSync(fences[currentBuffer]);
        fences[currentBuffer] = nullptr;
    }

    auto positions = scene.positions();
    std::memcpy(mappedPtr[currentBuffer], positions.data(), positions.size() * sizeof(ParticleVector));

    GLuint count = static_cast<GLuint>(positions.size());

    // --- Clear the accumulation texture ---
    static const GLuint zero = 0;
    glClearTexImage(densityTexture, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    // --- Splat pass: compute shader, one thread per particle ---
    glUseProgram(splatProgram);
    glUniform1ui(particleCountLoc_, count);
    glUniform2i(screenSizeLoc_, w, h);
    glUniformMatrix4fv(viewProjLoc_, 1, GL_FALSE, glm::value_ptr(viewProj));

    // Anchoring the reference depth to the camera's standoff keeps the centre of the
    // cloud at full brightness no matter how far the camera pulls back.
    glm::mat4 view = camera_.view();

    glUniform1f(depthFalloffLoc_, depthFalloff_);
    glUniform1f(depthReferenceLoc_, camera_.depthReference());
    glUniform4f(viewRowZLoc_, view[0][2], view[1][2], view[2][2], view[3][2]);
    glUniform1i(particleRadiusLoc_, particleRadius_);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO[currentBuffer]);
    glBindImageTexture(1, densityTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

    constexpr GLuint kLocalSize = 256;
    GLuint           groups     = (count + kLocalSize - 1) / kLocalSize;
    if (groups > 0) {
        // A dispatch is capped per dimension — 65535 on any D3D12-backed driver, which
        // is only ~16.7M particles down a single axis. Past the cap the driver rejects
        // the whole dispatch with GL_INVALID_VALUE and splats nothing, so the screen
        // goes black instead of degrading. Folding the excess into Y buys 65535x room;
        // the shader linearizes the grid back into a particle index.
        GLuint groupsX = std::min(groups, maxWorkGroups_);
        GLuint groupsY = (groups + groupsX - 1) / groupsX;
        glDispatchCompute(groupsX, groupsY, 1);
    }

    // Make sure the atomic writes are visible before the resolve pass reads them, and
    // before next frame's glClearTexImage overwrites them — image stores are incoherent
    // in both directions.
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);

    // --- Resolve pass: one fullscreen triangle, density -> color ---
    glUseProgram(resolveProgram);
    glUniform4fv(colorLoc_, 1, glm::value_ptr(particleColor_));
    glUniform1f(fadeLoc_, fadeScale_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture);
    glUniform1i(densitySamplerLoc_, 0);

    glBindVertexArray(fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    fences[currentBuffer] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}
