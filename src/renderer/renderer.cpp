#include "renderer.hpp"

#include "emitter/emitter.hpp"
#include "utils/opengl.hpp"
#include "utils/shader_cache.hpp"
#include "utils/utils.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>

namespace
{

constexpr std::size_t BufferCount {3};

GLuint      splatProgram;    // compute: particle positions -> density image
GLuint      resolveProgram;  // fullscreen: density image -> color
GLuint      fullscreenVAO;   // empty VAO, fullscreen triangle uses gl_VertexID
GLuint      densityTexture;
GLuint      hueTexture; // density-weighted hue sum, resolved against densityTexture
GLuint      particleSSBO[BufferCount];
GLuint      ageSSBO[BufferCount];
void*       mappedPtr[BufferCount];
void*       ageMappedPtr[BufferCount];
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

std::filesystem::path resource_string = PARTICLES_STRINGIFY(RESOURCE_DIR);

constexpr glm::vec3 kOrbitTarget {0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kWorldUp {0.0F, 1.0F, 0.0F};

} // namespace

glm::vec3 Camera::forward() const noexcept
{
    return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
}

glm::vec3 Camera::eye() const noexcept
{
    return kOrbitTarget - forward() * distance;
}

glm::mat4 Camera::viewProj(float aspect) const noexcept
{
    glm::mat4 proj = glm::perspective(glm::radians(fovDegrees), aspect, 0.01F, 1000.0F);
    return proj * glm::lookAt(eye(), kOrbitTarget, kWorldUp);
}

Renderer::~Renderer()
{
    for (std::size_t i = 0; i < BufferCount; ++i) {
        if (fences[i]) {
            glDeleteSync(fences[i]);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO[i]);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ageSSBO[i]);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    glDeleteBuffers(BufferCount, particleSSBO);
    glDeleteBuffers(BufferCount, ageSSBO);
    glDeleteTextures(1, &densityTexture);
    glDeleteTextures(1, &hueTexture);
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
    elapsedTimeLoc_    = glGetUniformLocation(splatProgram, "elapsedTime");
    cycleRateLoc_      = glGetUniformLocation(splatProgram, "colorCycleRate");

    colorLoc_          = glGetUniformLocation(resolveProgram, "particleColor");
    densitySamplerLoc_ = glGetUniformLocation(resolveProgram, "densityImage");
    hueSamplerLoc_     = glGetUniformLocation(resolveProgram, "hueImage");
    fadeLoc_           = glGetUniformLocation(resolveProgram, "fadeScale");
    paletteALoc_       = glGetUniformLocation(resolveProgram, "paletteA");
    paletteBLoc_       = glGetUniformLocation(resolveProgram, "paletteB");
    paletteCLoc_       = glGetUniformLocation(resolveProgram, "paletteC");
    paletteDLoc_       = glGetUniformLocation(resolveProgram, "paletteD");
    colorMixLoc_       = glGetUniformLocation(resolveProgram, "colorMix");
    coreWhitenLoc_     = glGetUniformLocation(resolveProgram, "coreWhiten");

    // Fullscreen triangle needs no vertex data at all — gl_VertexID drives it.
    glGenVertexArrays(1, &fullscreenVAO);

    // --- Particle storage buffers, persistent-mapped, used as SSBOs now ---
    glGenBuffers(BufferCount, particleSSBO);
    glGenBuffers(BufferCount, ageSSBO);

    for (std::size_t i = 0; i < BufferCount; ++i) {
        mappedPtr[i]    = createMappedStorage(particleSSBO[i], MAX_PARTICLES * sizeof(ParticleVector));
        ageMappedPtr[i] = createMappedStorage(ageSSBO[i], MAX_PARTICLES * sizeof(float));
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

    if (densityTexture) {
        glDeleteTextures(1, &densityTexture);
        glDeleteTextures(1, &hueTexture);
    }

    densityTexture = createUintTexture(w, h);
    hueTexture     = createUintTexture(w, h);
}

void Renderer::render(GLFWwindow* window)
{
    static auto t1 = Time::measure();
    auto        t2 = Time::measure();
    double      dt = std::max(0.01, (double)Time::duration<std::chrono::nanoseconds>(t1, t2).count());
    t1  = t2;
    dt /= 1e9;

    elapsedTime_ += dt;

    ImGui::Begin("Particle Settings");
    ImGui::Text("FPS: %f", 1.0 / dt);
    ImGui::Text("DT: %f", dt);
    static float fadeScale = 0.15f;
    ImGui::SliderFloat("Density fade", &fadeScale, 0.01f, 1.0f);
    ImGui::End();
    emitter.renderSettings();

    ImGui::Begin("Camera");
    ImGui::SliderAngle("Yaw", &camera_.yaw, -180.0F, 180.0F);
    ImGui::SliderAngle("Pitch", &camera_.pitch, -89.0F, 89.0F);
    ImGui::SliderFloat("Distance", &camera_.distance, 0.1F, 20.0F);
    ImGui::SliderFloat("FOV", &camera_.fovDegrees, 10.0F, 120.0F, "%.0f deg");
    static bool autoOrbit = false;
    ImGui::Checkbox("Auto orbit", &autoOrbit);
    ImGui::End();

    static float depthFalloff = 1.5F;

    ImGui::Begin("Nebula");
    ImGui::SliderFloat("Depth falloff", &depthFalloff, 0.0F, 3.0F);
    ImGui::SetItemTooltip("0 = flat, 1 = 1/depth, 2 = inverse-square");
    ImGui::SliderFloat("Color cycle", &palette_.cycleRate, 0.0F, 0.5F, "%.3f rev/s");
    ImGui::SliderFloat("Color mix", &palette_.mix, 0.0F, 1.0F);
    ImGui::SliderFloat("Core whiten", &palette_.coreWhiten, 0.0F, 1.0F);

    if (ImGui::TreeNode("Gradient (a + b*cos(2pi*(c*t + d)))")) {
        ImGui::DragFloat3("a  midpoint", glm::value_ptr(palette_.a), 0.01F, 0.0F, 1.0F);
        ImGui::DragFloat3("b  amplitude", glm::value_ptr(palette_.b), 0.01F, 0.0F, 1.0F);
        ImGui::DragFloat3("c  frequency", glm::value_ptr(palette_.c), 0.01F, 0.0F, 4.0F);
        ImGui::DragFloat3("d  phase", glm::value_ptr(palette_.d), 0.01F, 0.0F, 1.0F);

        // Strip of the gradient itself, so tweaking the coefficients is not blind.
        ImDrawList* draw  = ImGui::GetWindowDrawList();
        ImVec2      start = ImGui::GetCursorScreenPos();
        float       width = ImGui::GetContentRegionAvail().x;

        constexpr int kSteps = 64;
        for (int i = 0; i < kSteps; ++i) {
            float     t   = (float)i / (float)kSteps;
            glm::vec3 rgb = palette_.a + palette_.b * glm::cos(6.28318530718F * ((palette_.c * t) + palette_.d));
            rgb           = glm::clamp(rgb, 0.0F, 1.0F);

            float x0 = start.x + (width * t);
            float x1 = start.x + (width * ((float)(i + 1) / (float)kSteps));
            draw->AddRectFilled({x0, start.y}, {x1, start.y + 24.0F}, ImGui::ColorConvertFloat4ToU32({rgb.r, rgb.g, rgb.b, 1.0F}));
        }

        ImGui::Dummy({width, 24.0F});
        ImGui::TreePop();
    }
    ImGui::End();

    if (autoOrbit) {
        camera_.yaw = std::fmod(camera_.yaw + ((float)dt * 0.4F), 6.28318530718F);
    }

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    float     aspect   = (h > 0) ? (float)w / (float)h : 1.0F;
    glm::mat4 viewProj = camera_.viewProj(aspect);

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2               mouse    = ImGui::GetMousePos();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        float                local_x  = mouse.x - viewport->Pos.x;
        float                local_y  = mouse.y - viewport->Pos.y;

        float ndc_x = (2.0f * local_x) / w - 1.0f;
        float ndc_y = 1.0f - (2.0f * local_y) / h;

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
        if (std::abs(denom) > 1e-6F) {
            float t = glm::dot(kOrbitTarget - origin, forward) / denom;
            if (t > 0.0F) {
                emitter.spawn(origin + (ray * t), (float)dt);
            }
        }
    }

    auto t_start = Time::measure();
    emitter.update(dt);
    auto t_update = Time::measure();

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

    // auto [data, length] = emitter.cull(1.f/1000.0);
    auto data   = emitter.data();
    auto length = emitter.aliveCount();
    std::memcpy(mappedPtr[currentBuffer], data, length * sizeof(ParticleVector));
    std::memcpy(ageMappedPtr[currentBuffer], emitter.ages(), length * sizeof(float));
    auto t_upload = Time::measure();

    GLuint count = static_cast<GLuint>(emitter.aliveCount());

    // --- Clear accumulation textures ---
    static const GLuint zero = 0;
    glClearTexImage(densityTexture, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glClearTexImage(hueTexture, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    // --- Splat pass: compute shader, one thread per particle ---
    glUseProgram(splatProgram);
    glUniform1ui(particleCountLoc_, count);
    glUniform2i(screenSizeLoc_, w, h);
    glUniformMatrix4fv(viewProjLoc_, 1, GL_FALSE, glm::value_ptr(viewProj));

    // Anchoring the reference depth to the orbit distance keeps the centre of the
    // cloud at full brightness no matter how far the camera pulls back.
    glUniform1f(depthFalloffLoc_, depthFalloff);
    glUniform1f(depthReferenceLoc_, camera_.distance);
    glUniform1f(elapsedTimeLoc_, (float)elapsedTime_);
    glUniform1f(cycleRateLoc_, palette_.cycleRate);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO[currentBuffer]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ageSSBO[currentBuffer]);
    glBindImageTexture(1, densityTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(3, hueTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

    constexpr GLuint kLocalSize = 256;
    GLuint           groups     = (count + kLocalSize - 1) / kLocalSize;
    if (groups > 0) {
        glDispatchCompute(groups, 1, 1);
    }

    // Make sure the atomic writes are visible before the resolve pass reads them.
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // --- Resolve pass: one fullscreen triangle, density -> color ---
    static ImVec4 color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);

    glUseProgram(resolveProgram);
    glUniform4f(colorLoc_, color.x, color.y, color.z, color.w);
    glUniform1f(fadeLoc_, fadeScale);

    glUniform3fv(paletteALoc_, 1, glm::value_ptr(palette_.a));
    glUniform3fv(paletteBLoc_, 1, glm::value_ptr(palette_.b));
    glUniform3fv(paletteCLoc_, 1, glm::value_ptr(palette_.c));
    glUniform3fv(paletteDLoc_, 1, glm::value_ptr(palette_.d));
    glUniform1f(colorMixLoc_, palette_.mix);
    glUniform1f(coreWhitenLoc_, palette_.coreWhiten);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture);
    glUniform1i(densitySamplerLoc_, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, hueTexture);
    glUniform1i(hueSamplerLoc_, 1);

    glBindVertexArray(fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glFinish();
    auto t_draw = Time::measure();

    spdlog::info(
        "update: {}ms  upload: {}ms  splat+draw: {}ms",
        Time::duration<std::chrono::microseconds>(t_start, t_update).count() / 1000.0,
        Time::duration<std::chrono::microseconds>(t_update, t_upload).count() / 1000.0,
        Time::duration<std::chrono::microseconds>(t_upload, t_draw).count() / 1000.0);

    fences[currentBuffer] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}
