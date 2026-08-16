#include "app.hpp"

#include "app/console.hpp"
#include "utils/utils.hpp"

#include <spdlog/fmt/chrono.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdio>
#include <utility>


App::App(std::string const& title, std::shared_ptr<ImGuiConsoleSink> log_sink)
    : window_{2560, 1440, title},
      console_{std::move(log_sink)}
{
    Settings& settings {Settings::getInstance()};
    settings.option<bool>("View", "Performance", true);
    settings.describe("View", "Performance", "The frame and simulation timings panel");
}

void App::run()
{
    while (!window_.shouldClose()) {
        window_.pollEvents();

        const float dt = clock_.sample();

        auto const start_time = Time::measure();

        auto const begin_frame_time     = Time::execution(&App::beginFrame, this);
        auto const console_update_time  = Time::execution(&OutputConsole::update, console_);
        auto const step_simulation_time = Time::execution(&App::stepSimulation, this, dt);
        auto const render_settings_time = Time::execution(&Scene::renderSettings, scene_);
        auto const render_stats_time    = Time::execution(&App::renderStats, this);
        auto const render_time          = Time::execution(&Renderer::render, renderer_, window_.size(), *scene_, dt);
        auto const console_render_time  = Time::execution(&OutputConsole::render, console_);
        auto const finish_frame_time    = Time::execution(&Window::endFrame, window_);

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
    simulationStepsTaken_ = 0;
    simulationFellBehind_ = false;

    if (simulationRate_.get() == 0) {
        return;
    }

    float const step = 1.0F / static_cast<float>(simulationRate_.get());
    physicsUpdateAccumulator_ += dt;

    while ((physicsUpdateAccumulator_ >= step) && (simulationStepsTaken_ < simulationStepLimit_.get())) {
        scene_->update(step);
        physicsUpdateAccumulator_ -= step;
        ++simulationStepsTaken_;
    }

    if (physicsUpdateAccumulator_ >= step) {
        physicsUpdateAccumulator_ = std::fmod(physicsUpdateAccumulator_, step);
        simulationFellBehind_     = true;
    }
}

void App::renderStats()
{
    if (!Settings::getInstance().peek<bool>("View", "Performance")->get()) {
        return;
    }

    ImGui::Begin("Performance");

    ImGui::Text("FPS: %.1f", 1.0F / clock_.get());
    ImGui::Text("Frame: %.3f ms", clock_.get() * 1000.0F);

    ImGui::SeparatorText("Simulation");

    simulationRate_.render();
    simulationStepLimit_.render();


    if (simulationRate_.get() > 0) {
        ImGui::Text("Step: %.3f ms", 1000.0F / static_cast<float>(simulationRate_.get()));
        ImGui::Text("Steps: %zu / %zu", simulationStepsTaken_, simulationStepLimit_.get());

        if (simulationFellBehind_) {
            ImGui::TextColored(ImVec4{1.0F, 0.6F, 0.2F, 1.0F}, "Step limit reached, dropping time");
        }
    }

    ImGui::End();
}

void App::beginFrame()
{
    window_.beginFrame();

    Settings::getInstance().render();

    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
}
