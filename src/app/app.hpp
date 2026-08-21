#ifndef YARR_APP_APP_HPP
#define YARR_APP_APP_HPP

#include "app/backend/window.hpp"
#include "app/console.hpp"
#include "app/settings.hpp"
#include "logic/simulation.hpp"
#include "renderer/renderer.hpp"
#include "utils/bases.hpp"
#include "utils/knob.hpp"
#include "utils/logger.hpp"
#include "utils/utils.hpp"

#include <memory>
#include <string>


class App : public Immovable<App>, private Logger<App>
{
public:
    App(std::string const& title, std::shared_ptr<ImGuiConsoleSink> log_sink);

    void run();

private:
    void beginFrame();

    void renderSceneSettings();
    void renderStats();

    Window window_;

    std::unique_ptr<Renderer>   renderer_ {new Renderer};
    std::unique_ptr<Simulation> simulation_ {new Simulation};
    DeltaTimeClock              clock_;

    OutputConsole console_;

    Knob<bool>& performanceVisible_ {
        Settings::getInstance().option<bool>("View", "Simulation", true, "The frame and simulation timings panel")
    };
};

#endif
