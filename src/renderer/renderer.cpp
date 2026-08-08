#include "renderer.hpp"

#include "utils/utils.hpp"

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{

struct Particle
{
    float x, y;
    float vx, vy;
    float life; // seconds remaining
};

constexpr std::size_t VAOcount {1};
constexpr std::size_t MAX_PARTICLES {100000000};

GLuint renderingProgram;
GLuint vao[VAOcount];
GLuint particleVBO;

std::vector<Particle> particles;

std::filesystem::path resource_string = PARTICLES_STRINGIFY(RESOURCE_DIR);
float starRadiusFactor(float angle, int spikes, float innerRatio)
{
    float segment = 2.0f * 3.14159f / spikes;
    float a       = std::fmod(angle, segment);
    if (a < 0.0f) {
        a += segment;           // fmod can return negative for negative angles
    }

    float t        = a / segment;    // 0..1 within this spike segment
    float triangle = 1.0f - std::fabs(t * 2.0f - 1.0f); // 0 at edges, 1 at spike tip

    return innerRatio + (1.0f - innerRatio) * triangle;
}

void spawnParticles(float ndc_x, float ndc_y, int count,
    float minSpeed, float maxSpeed, float life,
    int spikes = 5, float innerRatio = 0.4f)
{
    for (int i = 0; i < count; ++i) {
        // Evenly space particles around the full circle so the star
        // shape is traced out cleanly rather than randomly.
        float angle = (static_cast<float>(i) / count) * 2.0f * 3.14159f;

        // Small jitter so it doesn't look laser-precise / mechanical.
        float jitter = ((rand() / (float)RAND_MAX) - 0.5f) * 0.05f;
        angle += jitter;

        float radiusFactor = starRadiusFactor(angle, spikes, innerRatio);
        float speed        = (minSpeed + (rand() / (float)RAND_MAX) * (maxSpeed - minSpeed))
            * radiusFactor;

        particles.push_back(
            {
                ndc_x, ndc_y,
                std::cos(angle) * speed, std::sin(angle) * speed,
                life
            });
    }
}

} // namespace

Renderer::Renderer()
{
    glEnable(GL_PROGRAM_POINT_SIZE);
    renderingProgram = createShaderProgram(
        {
            .vertex   = resource_string / "shaders/vertexShader.glsl",
            .fragment = resource_string / "shaders/fragmentShader.glsl"
        });

    glGenVertexArrays(VAOcount, vao);
    glBindVertexArray(vao[0]);

    glGenBuffers(1, &particleVBO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, x));
    glEnableVertexAttribArray(0);
}

void Renderer::render(GLFWwindow* window)
{
    static auto t1 = Time::measure();
    auto        t2 = Time::measure();
    double      dt = Time::duration(t1, t2).count() / 1000.0; // seconds elapsed
    t1 = t2;

    // Tunable settings — static so they persist across frames without
    // needing new Renderer member variables. Move these into the class
    // if you want them saved/loaded or touched from elsewhere.
    static float spawnRate    = 2000.0f;     // particles per second
    static float minSpeed     = 0.2f;
    static float maxSpeed     = 0.5f;
    static float particleLife = 1.0f;       // seconds
    static float pointSize    = 30.0f;
    static float gravity      = 0.0f;       // NDC units/sec^2, negative = downward pull
    static bool  emitEnabled  = true;
    static float elapsedTime  = 0.0f;
    static float hueSpeed     = 0.15f;
    static float saturation   = 0.8f;
    static float brightness   = 1.0f;
    elapsedTime += static_cast<float>(dt);
    static float particleColor[4] = {1.0f, 0.6f, 0.2f, 1.0f};

    ImGui::Begin("Particle Settings");
    static int   starSpikes     = 5;
    static float starInnerRatio = 0.4f;

    ImGui::SliderInt("Star spikes", &starSpikes, 3, 12);
    ImGui::SliderFloat("Star inner ratio", &starInnerRatio, 0.05f, 0.95f);
    ImGui::Checkbox("Emit on click", &emitEnabled);
    ImGui::SliderFloat("Spawn rate", &spawnRate, 0.0f, 50000.0f, "%.0f /sec");
    ImGui::SliderFloat("Min speed", &minSpeed, 0.0f, 2.0f);
    ImGui::SliderFloat("Max speed", &maxSpeed, minSpeed, 2.0f);
    ImGui::SliderFloat("Lifetime", &particleLife, 0.1f, 50.0f, "%.2f s");
    ImGui::SliderFloat("Point size", &pointSize, 1.0f, 100.0f);
    ImGui::SliderFloat("Gravity", &gravity, -2.0f, 2.0f);
    ImGui::ColorEdit4("Color", particleColor);
    ImGui::SliderFloat("Hue speed", &hueSpeed, 0.0f, 2.0f, "%.2f cycles/sec");
    ImGui::SliderFloat("Saturation", &saturation, 0.0f, 1.0f);
    ImGui::SliderFloat("Brightness", &brightness, 0.0f, 1.0f);
    ImGui::Text("Active particles: %zu", particles.size());
    if (ImGui::Button("Clear all")) {
        particles.clear();
    }

    ImGui::End();

    ImGuiIO&      io               = ImGui::GetIO();
    static double spawnAccumulator = 0.0;

    if (emitEnabled && !io.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2               mouse    = ImGui::GetMousePos();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        float local_x = mouse.x - viewport->Pos.x;
        float local_y = mouse.y - viewport->Pos.y;

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        float ndc_x = (2.0f * local_x) / w - 1.0f;
        float ndc_y = 1.0f - (2.0f * local_y) / h;

        spawnAccumulator += spawnRate * dt;
        int toSpawn = static_cast<int>(spawnAccumulator);
        spawnAccumulator -= toSpawn;

        if (toSpawn > 0) {
            spawnParticles(
                ndc_x,
                ndc_y,
                toSpawn,
                minSpeed,
                maxSpeed,
                particleLife,
                starSpikes,
                starInnerRatio);
        }
    }

    for (auto& p : particles) {
        p.vy   += gravity * dt;
        p.x    += p.vx * dt;
        p.y    += p.vy * dt;
        p.life -= dt;
    }

    std::erase_if(particles, [] (Particle const& p) { return p.life <= 0.0f; });

    glUseProgram(renderingProgram);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, particles.size() * sizeof(Particle), particles.data());

    glUniform1f(glGetUniformLocation(renderingProgram, "time"), elapsedTime);
    glUniform1f(glGetUniformLocation(renderingProgram, "hueSpeed"), hueSpeed);
    glUniform1f(glGetUniformLocation(renderingProgram, "saturation"), saturation);
    glUniform1f(glGetUniformLocation(renderingProgram, "brightness"), brightness);

    glBindVertexArray(vao[0]);
    glPointSize(pointSize);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particles.size()));
}
