#include "app.hpp"

#include "app/console.hpp"
#include "utils/utils.hpp"

#include <spdlog/fmt/chrono.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>


namespace
{

constexpr int kMaxStepsPerFrame = 8;

} // namespace


App::App(std::string const& title)
    : window_{2560, 1440, title}
{}

void App::run()
{
    while (!window_.shouldClose()) {
        const float dt         = clock.sample();
        auto const  start_time = Time::measure();

        auto const begin_frame_time     = Time::execution(&App::beginFrame, this);
        auto const console_update_time  = Time::execution(&OutputConsole::update, console);
        auto const step_simulation_time = Time::execution(&App::stepSimulation, this, dt);
        auto const render_settings_time = Time::execution(&Scene::renderSettings, scene_);
        auto const render_stats_time    = Time::execution(&App::renderStats, this);
        auto const render_time          = Time::execution(&Renderer::render, renderer_, window_.size(), *scene_, dt);
        auto const console_render_time  = Time::execution(&OutputConsole::render, console);
        auto const finish_frame_time    = Time::execution(&App::finishFrame, this);

        auto const end_time = Time::measure();


        spdlog::debug("Begin frame time: {}", begin_frame_time);
        spdlog::debug("Console update time: {}", console_update_time);
        spdlog::debug("Step simulation time: {}", step_simulation_time);
        spdlog::debug("Render settings time: {}", render_settings_time);
        spdlog::debug("Render stats time: {}", render_stats_time);
        spdlog::debug("Render time: {}", render_time);
        spdlog::debug("Console render time: {}", console_render_time);
        spdlog::debug("Finish frame time: {}", finish_frame_time);
        spdlog::debug("Full loop timing: {}", Time::duration(start_time, end_time));
    }
}

void App::stepSimulation(float dt)
{
    if (simulationRate_.get() == 0) {
        return;
    }

    float const step = 1.0F / static_cast<float>(simulationRate_.get());
    physicsUpdateAccumulator_ += dt;

    while (physicsUpdateAccumulator_ > 0.0F) {
        scene_->update(step);
        physicsUpdateAccumulator_ -= step;
    }
}

void App::renderStats()
{
    ImGui::Begin("Performance");

    ImGui::Text("FPS: %.1f", 1.0F / clock.get());
    ImGui::Text("Frame: %.3f ms", clock.get() * 1000.0F);

    ImGui::SeparatorText("Simulation");

    simulationRate_.render();
    ImGui::SetItemTooltip("How often the simulation steps, independent of the frame rate");
    ImGui::Text("Step: %.3f ms", 1000.0F / (float)simulationRate_.get());

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

void App::beginFrame()
{
    window_.pollEvents();
    window_.clear();
    imgui_.newFrame();

    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
}

void App::finishFrame()
{
    auto const [display_w, display_h] = window_.size();
    glViewport(0, 0, display_w, display_h);

    imgui_.render();
    window_.swapBuffers();
}
