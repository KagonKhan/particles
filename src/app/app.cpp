#include "app.hpp"

#include "utils/opengl.hpp"
#include "utils/utils.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cstdio>


#include "utils/shader_cache.hpp"

namespace
{

const ImGuiWindowFlags window_flags =
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
;


constexpr float kFrameTimeSmoothing = 0.05F;
constexpr int   kMaxStepsPerFrame   = 8;
constexpr float kBenchmarkUiPeriod  = 0.1F;

} // namespace


App::App(std::string const& title)
    : window_{2560, 1440, title}
{}

void App::run()
{
    auto previousFrame = Time::measure();

    while (!window_.shouldClose()) {
        auto start_time = Time::measure();

        // Floored, so a frame the clock reports as instantaneous cannot divide by zero
        // in the FPS readout or stall the spawn accumulator.
        auto  now = Time::measure();
        float dt  = std::max(
            1e-6F,
            static_cast<float>(Time::duration<std::chrono::nanoseconds>(previousFrame, now).count()) / 1e9F);
        previousFrame       = now;
        smoothedFrameTime_ += (dt - smoothedFrameTime_) * kFrameTimeSmoothing;

        window_.pollEvents();
        if (window_.iconified()) {
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

        if (scene_->benchmarking() && (uiAccumulator_ < kBenchmarkUiPeriod)) {
            // Slept rather than spun, so the one thread left awake is not competing either.
            ImGui_ImplGlfw_Sleep(1);
            continue;
        }

        uiAccumulator_ = 0.0F;

        glClearColor(0.0, 0.0, 0.0, 1.0);
        // Depth too: the body pass is the one thing that depth tests, and a depth
        // buffer left over from the previous frame would occlude this frame's bodies.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        imgui_.newFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("ImGui Template", nullptr, window_flags);
        ImGui::PopStyleVar(1);

        ImGuiID dockspace_id = ImGui::GetID("RootDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        spdlog::debug("Scene settings render time: {}ms", Time::execution(&Scene::renderSettings, scene_).count());
        renderStats();

        // The scene pass is the expensive one — an upload of the whole pool and a million
        // particles rasterized — and it is the one a run does without entirely.
        if (!scene_->benchmarking()) {
            auto const [width, height] = window_.framebufferSize();
            spdlog::debug(
                "Rendering time: {}ms",
                Time::execution(&Renderer::render, renderer_, width, height, *scene_, dt).count());
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
        spdlog::debug("Scene update time: {}ms", Time::execution(&Scene::update, scene_, step).count());
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
    if (scene_->benchmarking()) {
        ImGui::SeparatorText("Benchmark");
        ImGui::TextColored(ImVec4 {1.0F, 0.8F, 0.3F, 1.0F}, "Recording — scene rendering paused");
        ImGui::SetItemTooltip(
            "The scene pass is skipped and the interface is drawn ten times a second, so the "
            "rasterizer is not competing with the simulation for cores. The frame readout above "
            "does not mean anything until the run ends.");
    }

    ImGui::End();
}

void App::finishFrame()
{
    auto const [display_w, display_h] = window_.framebufferSize();
    glViewport(0, 0, display_w, display_h);

    imgui_.render();
    window_.swapBuffers();
}
