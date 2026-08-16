#ifndef YARR_APP_APP_HPP
#define YARR_APP_APP_HPP

#include "app/backend/imgui_layer.hpp"
#include "app/backend/window.hpp"
#include "app/console.hpp"
#include "app/settings.hpp"
#include "logic/knob.hpp"
#include "logic/scene.hpp"
#include "renderer/renderer.hpp"
#include "utils/bases.hpp"
#include "utils/utils.hpp"

#include <memory>
#include <string>


class App : public Immovable<App>
{
public:
    App(std::string const& title);

    void run();

private:
    void beginFrame();
    void finishFrame();

    void stepSimulation(float dt);
    void renderStats();

    Window     window_;
    ImGuiLayer imgui_ {window_};

    Settings&   settings_ {Settings::getInstance()};
    Knob<bool>& showPerformance_ {settings_.option<bool>("View", "Performance", true)};
    Knob<int>&  simulationRate_ {settings_.option<int>("Simulation", "Rate", 120, 0, 480, "%d Hz")};
    Knob<bool>& vsync_ {settings_.option<bool>("Window", "VSync", false)};

    std::unique_ptr<Renderer> renderer_ {new Renderer};
    std::unique_ptr<Scene>    scene_ {new Scene};
    DeltaTimeClock            clock;

    OutputConsole console;

    float             physicsUpdateAccumulator_ {0.0F};
    Knob<std::size_t> simulationStepLimit_ {"Simulation Step Limit", 8, 1, 16, "%d steps"};
};

#endif
