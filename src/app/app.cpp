#include "app.hpp"

#include "app/console.hpp"
#include "scene_layout.hpp"
#include "utils/utils.hpp"


#include <imgui.h>
#include <chrono>
#include <thread>
#include <utility>


namespace
{

constexpr auto FRAME_PERIOD = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::duration<double> {1.0 / 60.0});

} // namespace

App::App(std::string const& title, std::shared_ptr<ImGuiConsoleSink> log_sink)
    : window_{2560, 1440, title},
      console_{std::move(log_sink)}
{}

void App::run()
{
    auto next_frame = std::chrono::steady_clock::now();

    while (!window_.shouldClose()) {
        window_.pollEvents();

        if (window_.iconified()) {
            window_.waitEvents();
            clock_.reset();
            continue;
        }

        const float dt = clock_.sample();

        auto const start_time = Time::measure();

        auto const begin_frame_time     = Time::execution(&App::beginFrame, this);
        auto const console_update_time  = Time::execution(&OutputConsole::update, console_);
        auto const render_settings_time = Time::execution(&App::renderSceneSettings, this);
        auto const render_stats_time    = Time::execution(&App::renderStats, this);
        auto const render_time          = Time::execution(
            &Renderer::render,
            renderer_,
            window_.size(),
            *simulation_,
            dt);
        auto const console_render_time = Time::execution(&OutputConsole::render, console_);
        auto const finish_frame_time   = Time::execution(&Window::endFrame, window_);

        auto const end_time = Time::measure();


        debug("Begin frame time: {}", begin_frame_time);
        debug("Console update time: {}", console_update_time);
        debug("Render settings time: {}", render_settings_time);
        debug("Render stats time: {}", render_stats_time);
        debug("Render time: {}", render_time);
        debug("Console render time: {}", console_render_time);
        debug("Finish frame time: {}", finish_frame_time);
        debug("Full loop timing: {}", Time::duration(start_time, end_time));

        next_frame += FRAME_PERIOD;

        if (auto const now = std::chrono::steady_clock::now(); (next_frame < now)) {
            next_frame = now;
        }
        else {
            std::this_thread::sleep_until(next_frame);
        }
    }
}

void App::renderSceneSettings()
{
    auto scene = simulation_->borrow();
    scene->renderSettings();
}

void App::renderStats()
{
    if (!performanceVisible_.get()) {
        return;
    }

    ImGui::Begin("Simulation");

    ImGui::SeparatorText("Rendering");
    ImGui::Text("FPS: %.1f", 1.0F / clock_.get());
    ImGui::SameLine();
    ImGui::Text("Frame: %.3f ms", clock_.get() * 1000.0F);

    ImGui::SeparatorText("Simulation");
    simulation_->renderSettings();

    ImGui::End();
}

void App::beginFrame()
{
    window_.beginFrame();

    Settings::getInstance().render();

    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
}
