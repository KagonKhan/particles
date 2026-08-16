#ifndef YARR_APP_APP_HPP
#define YARR_APP_APP_HPP

#include "app/backend/window.hpp"
#include "app/console.hpp"
#include "app/settings.hpp"
#include "logic/scene.hpp"
#include "renderer/renderer.hpp"
#include "utils/bases.hpp"
#include "utils/knob.hpp"
#include "utils/utils.hpp"

#include <memory>
#include <string>


class App : public Immovable<App>
{
public:
    App(std::string const& title, std::shared_ptr<ImGuiConsoleSink> logSink);

    void run();

private:
    void beginFrame();

    void stepSimulation(float dt);
    void renderStats();

    Window window_;

    std::unique_ptr<Renderer> renderer_ {new Renderer};
    std::unique_ptr<Scene>    scene_ {new Scene};
    DeltaTimeClock            clock;

    Knob<int> simulationRate_ {
        "Simulation Rate", 120, 0, 480, "%d Hz",
        "How often the simulation steps, independent of the frame rate"
    };
    Knob<std::size_t> simulationStepLimit_ {
        "Simulation Step Limit", 8, 1, 16, "%zu steps",
        "Most steps one frame may take. Holding real time needs rate / FPS of them;\nbelow that the simulation runs slow rather than stalling the interface."
    };

    OutputConsole console;

    float       physicsUpdateAccumulator_ = 0.0F;
    std::size_t simulationStepsTaken_     = 0;
    bool        simulationFellBehind_     = false;
};

#endif
