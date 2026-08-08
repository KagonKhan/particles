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

GLuint renderingProgram;
GLuint vao[VAOcount];
GLuint particleVBO;


std::filesystem::path resource_string = PARTICLES_STRINGIFY(RESOURCE_DIR);

} // namespace

Renderer::Renderer()
{
    // glEnable(GL_PROGRAM_POINT_SIZE);
    renderingProgram = createShaderProgram(
        {
            .vertex   = resource_string / "shaders/vertexShader.glsl",
            .fragment = resource_string / "shaders/fragmentShader.glsl"
        });

    glGenVertexArrays(VAOcount, vao);
    glBindVertexArray(vao[0]);

    glGenBuffers(1, &particleVBO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, Emitter::MAX_PARTICLES * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, x));
    glEnableVertexAttribArray(0);
}

void Renderer::render(GLFWwindow* window)
{
    static auto t1 = Time::measure();
    auto        t2 = Time::measure();
    double      dt = std::max(0.01, (double)Time::duration<std::chrono::nanoseconds>(t1, t2).count() ); // seconds elapsed
    t1  = t2;
    dt /= 1e9;

    static float pointSize = 3.0f;

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

    emitter.update(dt);

    glUseProgram(renderingProgram);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, emitter.aliveCount() * sizeof(Particle), emitter.data());


    glBindVertexArray(vao[0]);
    glPointSize(pointSize);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(emitter.aliveCount()));
}
