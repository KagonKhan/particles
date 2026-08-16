#ifndef YARR_APP_APP_HPP
#define YARR_APP_APP_HPP

#include "app/console.hpp"
#include "app/imgui_layer.hpp"
#include "app/scene.hpp"
#include "app/window.hpp"
#include "logic/knob.hpp"
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

    Window                    window_;
    ImGuiLayer                imgui_ {window_};
    std::unique_ptr<Renderer> renderer_ {new Renderer};
    std::unique_ptr<Scene>    scene_ {new Scene};
    DeltaTimeClock            clock;

    OutputConsole console;

    Knob<int> simulationRate_ {"Simulation Rate", 120, 0, 480, "%d Hz"};
    float     physicsUpdateAccumulator_ {0.0F};
};

#endif
