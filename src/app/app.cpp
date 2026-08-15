#include "app.hpp"

#include "exceptions.hpp"

#include "utils/opengl.hpp"
#include "utils/utils.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string_view>


#include "utils/shader_cache.hpp"

namespace
{

std::filesystem::path  resource_string = PARTICLES_STRINGIFY(RESOURCE_DIR);
const char* const      glsl_version    = "#version 440";
const ImGuiWindowFlags window_flags    =
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
;


// How much of a frame's own timing shows up in the readout. Low enough that a single slow
// frame nudges the number rather than throwing it.
constexpr float kFrameTimeSmoothing = 0.05F;

// A frame that cannot afford this many simulation steps stops trying to catch up. Without
// a ceiling, a frame that runs long asks for more steps, which makes the next frame run
// longer still.
constexpr int kMaxStepsPerFrame = 8;

// How often the interface is drawn while a benchmark is recording. Rare enough that the
// rasterizer is effectively absent from the run, often enough that the progress bar moves and
// the cancel button answers.
constexpr float kBenchmarkUiPeriod = 0.1F;


void glfw_error_callback(int error, const char* description)
{
    spdlog::error("Glfw Error {}: {}\n", error, description);
}

} // namespace


App::App(std::string const& title)
{
    // ===   GLFW INITIALIZATION   =====================================================================================
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        throw InitializationError("glfwInit failed!");
    }


    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);   // 3.2+
    //  only glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // 3.0+ only

    window = glfwCreateWindow(2560, 1440, title.c_str(), NULL, NULL);
    if (window == NULL) {
        throw InitializationError("Could not create a window");
    }

    glfwMakeContextCurrent(window);
    if (glewInit() != GLEW_OK) {
        exit(EXIT_FAILURE);
    }

    // Which driver answered, said out loud. A software rasterizer is a working renderer and
    // announces itself no other way — it cost a day of benchmarking the wrong thing once.
    auto const* const renderer_name = reinterpret_cast<char const*>(glGetString(GL_RENDERER));
    spdlog::info(
        "GL renderer: {} | {}",
        renderer_name,
        reinterpret_cast<char const*>(glGetString(GL_VERSION)));

    if (std::string_view {renderer_name}.contains("llvmpipe")) {
        spdlog::warn("Rendering in software. Expect ~10x the CPU, on threads that compete with the simulation.");
    }

    glfwSwapInterval(0);
    // glfwSwapInterval(1); // Enable vsync
    glEnable(GL_BLEND);

    // ===   IMGUI INITIALIZATION   ====================================================================================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;                   // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;                    // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;                       // Enable Docking
    // io.ConfigFlags                 |= ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport
    io.ConfigViewportsNoAutoMerge   = true;
    io.ConfigViewportsNoTaskBarIcon = true;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding              = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);


    // ===   SHADER INITIALIZATION   ===================================================================================
    ShaderCache::compileProgram(
        "PointProgram",
        {
            ShaderCache::load(
                "point_vertex",
                {
                    .source = resource_string / "shaders/point.vert",
                    .type   = GL_VERTEX_SHADER
                }),
            ShaderCache::load(
                "point_fragment",
                {
                    .source = resource_string / "shaders/point.frag",
                    .type   = GL_FRAGMENT_SHADER
                }),
        }
    );

    ShaderCache::compileProgram(
        "ShapeProgram",
        {
            ShaderCache::load(
                "shape_vertex",
                {
                    .source = resource_string / "shaders/shape.vert",
                    .type   = GL_VERTEX_SHADER
                }),
            ShaderCache::load(
                "shape_fragment",
                {
                    .source = resource_string / "shaders/shape.frag",
                    .type   = GL_FRAGMENT_SHADER
                }),
        }
    );

    ShaderCache::compileProgram(
        "SplatProgram",
        {
            ShaderCache::load(
                "splat_compute",
                {
                    .source = resource_string / "shaders/splat.comp",
                    .type   = GL_COMPUTE_SHADER
                })
        }
    );

    ShaderCache::compileProgram(
        "ResolveProgram",
        {
            ShaderCache::load(
                "fullscreen_vertex",
                {
                    .source = resource_string / "shaders/fullscreen.vert",
                    .type   = GL_VERTEX_SHADER
                }),
            ShaderCache::load(
                "density_fragment",
                {
                    .source = resource_string / "shaders/density.frag",
                    .type   = GL_FRAGMENT_SHADER
                }),
        }
    );


    // TODO: Fix the ordering problems. Shader cache needs to load before renderer
    renderer = new Renderer();
}

App::~App()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void App::run()
{
    auto previousFrame = Time::measure();

    while (!glfwWindowShouldClose(window)) {
        auto start_time = Time::measure();

        // Floored, so a frame the clock reports as instantaneous cannot divide by zero
        // in the FPS readout or stall the spawn accumulator.
        auto  now = Time::measure();
        float dt  = std::max(
            1e-6F,
            static_cast<float>(Time::duration<std::chrono::nanoseconds>(previousFrame, now).count()) / 1e9F);
        previousFrame       = now;
        smoothedFrameTime_ += (dt - smoothedFrameTime_) * kFrameTimeSmoothing;

        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Ahead of the frame, and outside it: the simulation is the thing being measured, and
        // one that stepped only when something was drawn would be reporting the renderer.
        spdlog::debug("Simulation step time: {}ms", Time::execution(&App::stepSimulation, this, dt).count());

        // A recording benchmark draws neither the scene nor, for the most part, the interface.
        // The rasterizer here is llvmpipe — sixteen software threads competing for the sixteen
        // the simulation is trying to measure — so a million particles drawn between steps
        // lands in every number the run produces. See docs/performance.md.
        uiAccumulator_ += dt;

        if (scene.benchmarking() && (uiAccumulator_ < kBenchmarkUiPeriod)) {
            // Slept rather than spun, so the one thread left awake is not competing either.
            ImGui_ImplGlfw_Sleep(1);
            continue;
        }

        uiAccumulator_ = 0.0F;

        glClearColor(0.0, 0.0, 0.0, 1.0);
        // Depth too: the body pass is the one thing that depth tests, and a depth
        // buffer left over from the previous frame would occlude this frame's bodies.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        startNewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("ImGui Template", nullptr, window_flags);
        ImGui::PopStyleVar(1);

        ImGuiID dockspace_id = ImGui::GetID("RootDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        spdlog::debug("Scene settings render time: {}ms", Time::execution(&Scene::renderSettings, scene).count());
        renderStats();

        // The scene pass is the expensive one — an upload of the whole pool and a million
        // particles rasterized — and it is the one a run does without entirely.
        if (!scene.benchmarking()) {
            spdlog::debug(
                "Rendering time: {}ms",
                Time::execution(&Renderer::render, renderer, window, scene, dt).count());
        }

        ImGui::Begin("Console Log");
        console.render();
        ImGui::End();

        ImGui::End();

        spdlog::debug("Frame finish time: {}ms", Time::execution(&App::finishFrame, this).count());

        auto end_time = Time::measure();
        spdlog::debug("Full loop timing: {}ms", Time::duration(start_time, end_time).count());
    }
}

void App::stepSimulation(float dt)
{
    // Every step is the same length whatever the frame took, so the simulation sees a
    // steady clock: a hitching frame becomes several steps rather than one enormous one,
    // and a frame rate far above the simulation rate becomes no step at all.
    float const step = 1.0F / simulationRate_;

    simulationAccumulator_ += dt;

    stepsLastFrame_ = 0;
    while ((simulationAccumulator_ >= step) && (stepsLastFrame_ < kMaxStepsPerFrame)) {
        spdlog::debug("Scene update time: {}ms", Time::execution(&Scene::update, scene, step).count());
        simulationAccumulator_ -= step;
        ++stepsLastFrame_;
    }

    // Only reachable by hitting the ceiling above. Keeping the backlog would spend every
    // later frame at the cap trying to work off time it can never make up, so the
    // simulation drops it and falls behind the wall clock instead.
    if (simulationAccumulator_ >= step) {
        simulationAccumulator_ = 0.0F;
    }
}

void App::renderStats()
{
    ImGui::Begin("Performance");

    ImGui::Text("FPS: %.1f", 1.0F / smoothedFrameTime_);
    ImGui::Text("Frame: %.3f ms", smoothedFrameTime_ * 1000.0F);

    ImGui::SeparatorText("Simulation");

    ImGui::SliderFloat("Rate", &simulationRate_, 1.0F, 480.0F, "%.0f Hz", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("How often the simulation steps, independent of the frame rate");
    ImGui::Text("Step: %.3f ms", 1000.0F / simulationRate_);
    ImGui::Text("Steps this frame: %d", stepsLastFrame_);

    // The window will look frozen while this is up, which is the point rather than a fault.
    if (scene.benchmarking()) {
        ImGui::SeparatorText("Benchmark");
        ImGui::TextColored(ImVec4 {1.0F, 0.8F, 0.3F, 1.0F}, "Recording — scene rendering paused");
        ImGui::SetItemTooltip(
            "The scene pass is skipped and the interface is drawn ten times a second, so the "
            "rasterizer is not competing with the simulation for cores. The frame readout above "
            "does not mean anything until the run ends.");
    }

    ImGui::End();
}

void App::startNewFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void App::finishFrame()
{
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window);
}
