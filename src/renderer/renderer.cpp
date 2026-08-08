#include "renderer.hpp"

#include "utils/utils.hpp"

#include <cmath>
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{

constexpr std::size_t VAOcount {1};
constexpr std::size_t BufferCount {3};

GLuint      renderingProgram;
GLuint      vao[VAOcount];
GLuint      particleVBO[BufferCount];
void*       mappedPtr[BufferCount]; // persistent CPU-visible pointers
GLsync      fences[BufferCount] = {}; // one fence per buffer, tracks "GPU done reading this"
std::size_t currentBuffer       = 0;
// A unit quad centered at origin, sized [-0.5, 0.5] on each axis.
// Scaled up in the vertex shader by point size.

std::filesystem::path resource_string = PARTICLES_STRINGIFY(RESOURCE_DIR);

} // namespace

Renderer::~Renderer()
{
    for (std::size_t i = 0; i < BufferCount; ++i) {
        if (fences[i]) {
            glDeleteSync(fences[i]);
        }

        glBindBuffer(GL_ARRAY_BUFFER, particleVBO[i]);
        glUnmapBuffer(GL_ARRAY_BUFFER); // must unmap before deleting a mapped buffer
    }

    glDeleteBuffers(BufferCount, particleVBO);
}

Renderer::Renderer()
{
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SMOOTH);   // allow to have rounded dots
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderingProgram = createShaderProgram(
        {
            .vertex   = resource_string / "shaders/vertexShader.glsl",
            .fragment = resource_string / "shaders/fragmentShader.glsl"
        });
    pointSizeLoc_ = glGetUniformLocation(renderingProgram, "pointSizePixels");
    colorLoc_     = glGetUniformLocation(renderingProgram, "particleColor");

    glGenVertexArrays(VAOcount, vao);
    glBindVertexArray(vao[0]);


    // divisor 0 (default) = advance per-vertex, i.e. all 4 quad corners are shared by every instance

    // --- Per-instance particle position: attribute location 1 ---
    glGenBuffers(BufferCount, particleVBO);

    GLbitfield storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    GLbitfield mapFlags     = storageFlags; // same flags for map as for storage

    for (std::size_t i = 0; i < BufferCount; ++i) {
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO[i]);
        glBufferStorage(GL_ARRAY_BUFFER, Emitter::MAX_PARTICLES * sizeof(ImVec2), nullptr, storageFlags);
        mappedPtr[i] = glMapBufferRange(GL_ARRAY_BUFFER, 0, Emitter::MAX_PARTICLES * sizeof(ImVec2), mapFlags);

        if (!mappedPtr[i]) {
            throw std::runtime_error("Failed to map particle buffer");
        }
    }

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ImVec2), (void*)offsetof(ImVec2, x));
    glEnableVertexAttribArray(0);
}

void Renderer::render(GLFWwindow* window)
{
    static auto t1 = Time::measure();
    auto        t2 = Time::measure();
    double      dt = std::max(0.01, (double)Time::duration<std::chrono::nanoseconds>(t1, t2).count() ); // seconds elapsed
    t1  = t2;
    dt /= 1e9;

    static float pointSize = 2.f;

    ImGui::Begin("Particle Settings");
    ImGui::Text("FPS: %f", 1.0/dt);
    ImGui::Text("DT: %f", dt);
    ImGui::End();
    emitter.renderSettings();

    ImGuiIO& io = ImGui::GetIO();

    if ( !io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2               mouse    = ImGui::GetMousePos();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        float local_x = mouse.x - viewport->Pos.x;
        float local_y = mouse.y - viewport->Pos.y;

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        float ndc_x = (2.0f * local_x) / w - 1.0f;
        float ndc_y = 1.0f - (2.0f * local_y) / h;

        emitter.spawn({ndc_x, ndc_y}, dt);
    }

    auto t_start = Time::measure();
    emitter.update(dt);
    auto t_update = Time::measure();


    currentBuffer = (currentBuffer + 1) % BufferCount;
    if (fences[currentBuffer]) {
        GLenum result = glClientWaitSync(
            fences[currentBuffer],
            GL_SYNC_FLUSH_COMMANDS_BIT,
            /*timeout ns*/ 1'000'000'000);
        if ((result == GL_TIMEOUT_EXPIRED) || (result == GL_WAIT_FAILED)) {
            spdlog::warn("particle buffer fence wait failed/timed out");
        }

        glDeleteSync(fences[currentBuffer]);
        fences[currentBuffer] = nullptr;
    }

    // Straight memcpy into mapped memory — no GL upload call needed at all.
    std::memcpy(mappedPtr[currentBuffer], emitter.data(), emitter.aliveCount() * sizeof(ImVec2));
    auto t_upload = Time::measure();

    glBindBuffer(GL_ARRAY_BUFFER, particleVBO[currentBuffer]);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ImVec2), (void*)offsetof(ImVec2, x));


    glUseProgram(renderingProgram);
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    static ImVec4 color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);  // orange, RGBA in [0,1]

    glUniform4f(colorLoc_, color.x, color.y, color.z, color.w);
// in render(), before the draw call:
    glUniform1f(pointSizeLoc_, pointSize); // pointSize is already in your local static float
    glBindVertexArray(vao[0]);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(emitter.aliveCount()));

    glFinish(); // force the draw to actually complete here, not just get queued
    auto t_draw = Time::measure();

    spdlog::info(
        "update: {}ms  upload: {}ms  draw: {}ms",
        Time::duration<std::chrono::microseconds>(t_start, t_update).count() / 1000.0,
        Time::duration<std::chrono::microseconds>(t_update, t_upload).count() / 1000.0,
        Time::duration<std::chrono::microseconds>(t_upload, t_draw).count() / 1000.0);
    fences[currentBuffer] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}
