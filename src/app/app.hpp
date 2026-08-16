#ifndef YARR_APP_APP_HPP
#define YARR_APP_APP_HPP

#include "app/console.hpp"
#include "app/imgui_layer.hpp"
#include "app/scene.hpp"
#include "app/window.hpp"
#include "renderer/renderer.hpp"
#include "utils/bases.hpp"

#include <memory>
#include <string>


class App : public Immovable<App>
{
public:
    App(std::string const& title);

    App(const App&)             = delete;
    App(App&&)                  = delete;
    App &operator =(const App&) = delete;
    App &operator =(App&&)      = delete;

    void run();

private:
    void finishFrame();

    void stepSimulation(float dt);
    void renderStats();

    Window                    window_;
    ImGuiLayer                imgui_ {window_};
    std::unique_ptr<Renderer> renderer_ {new Renderer};
    std::unique_ptr<Scene>    scene_ {new Scene};

    OutputConsole console;

    float smoothedFrameTime_     = 1.0F / 60.0F;
    float simulationRate_        = 60.0F;
    float simulationAccumulator_ = 0.0F;
    int   stepsLastFrame_        = 0;

    // Time since the interface was last drawn. Only consulted while a benchmark is
    // recording, when frames are deliberately rare.
    float uiAccumulator_ {0.0F};
};

#endif
