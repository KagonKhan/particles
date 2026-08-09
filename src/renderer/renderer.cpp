#include "renderer.hpp"

#include "utils/opengl.hpp"
#include "utils/shader_cache.hpp"
#include "utils/utils.hpp"

#include <cmath>
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
GLuint      particleSSBO[BufferCount];
void*       mappedPtr[BufferCount];
GLsync      fences[BufferCount] = {};
std::size_t currentBuffer       = 0;

std::filesystem::path resource_string = PARTICLES_STRINGIFY(RESOURCE_DIR);

} // namespace

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

    colorLoc_          = glGetUniformLocation(resolveProgram, "particleColor");
    densitySamplerLoc_ = glGetUniformLocation(resolveProgram, "densityImage");
    fadeLoc_           = glGetUniformLocation(resolveProgram, "fadeScale");

    // Fullscreen triangle needs no vertex data at all — gl_VertexID drives it.
    glGenVertexArrays(1, &fullscreenVAO);

    // --- Particle position storage buffers, persistent-mapped, used as SSBOs now ---
    glGenBuffers(BufferCount, particleSSBO);
    GLbitfield storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

    for (std::size_t i = 0; i < BufferCount; ++i) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO[i]);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(ImVec2), nullptr, storageFlags);
        mappedPtr[i] = glMapBufferRange(
            GL_SHADER_STORAGE_BUFFER,
            0,
            MAX_PARTICLES * sizeof(ImVec2),
            storageFlags);

        if (!mappedPtr[i]) {
            throw std::runtime_error("Failed to map particle buffer");
        }
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
    }

    glGenTextures(1, &densityTexture);
    glBindTexture(GL_TEXTURE_2D, densityTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void Renderer::render(GLFWwindow* window)
{
    static auto t1 = Time::measure();
    auto        t2 = Time::measure();
    double      dt = std::max(0.01, (double)Time::duration<std::chrono::nanoseconds>(t1, t2).count());
    t1  = t2;
    dt /= 1e9;

    ImGui::Begin("Particle Settings");
    ImGui::Text("FPS: %f", 1.0 / dt);
    ImGui::Text("DT: %f", dt);
    static float fadeScale = 0.15f;
    ImGui::SliderFloat("Density fade", &fadeScale, 0.01f, 1.0f);
    ImGui::End();
    emitter.renderSettings();

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2               mouse    = ImGui::GetMousePos();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        float                local_x  = mouse.x - viewport->Pos.x;
        float                local_y  = mouse.y - viewport->Pos.y;

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        float ndc_x = (2.0f * local_x) / w - 1.0f;
        float ndc_y = 1.0f - (2.0f * local_y) / h;
        emitter.spawn({ndc_x, ndc_y}, dt);
    }

    auto t_start = Time::measure();
    emitter.update(dt);
    auto t_update = Time::measure();

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
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
    std::memcpy(mappedPtr[currentBuffer], data, length * sizeof(ImVec2));
    auto t_upload = Time::measure();

    GLuint count = static_cast<GLuint>(emitter.aliveCount());

    // --- Clear density texture ---
    static const GLuint zero = 0;
    glClearTexImage(densityTexture, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    // --- Splat pass: compute shader, one thread per particle ---
    glUseProgram(splatProgram);
    glUniform1ui(particleCountLoc_, count);
    glUniform2i(screenSizeLoc_, w, h);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO[currentBuffer]);
    glBindImageTexture(1, densityTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, densityTexture);
    glUniform1i(densitySamplerLoc_, 0);

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
