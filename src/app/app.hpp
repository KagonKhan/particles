#ifndef YARR_APP_APP_HPP
#define YARR_APP_APP_HPP
 #define GLM_ENABLE_EXPERIMENTAL
#include "app/console.hpp"
#include "app/scene.hpp"
#include "renderer/renderer.hpp"

#include <GLFW/glfw3.h>

#include <string>

class App
{
public:
    App(std::string const& title);
    ~App();

    void run();

private:
    void initializeGLFW(std::string const& window_name);
    void initializeIMGUI();

    void startNewFrame();
    void finishFrame();

    void stepSimulation(float dt);
    void renderStats();

    GLFWwindow*   window;
    Renderer*     renderer;
    Scene         scene;
    OutputConsole console;

    // Exponentially smoothed, because a readout driven by a single frame's dt flickers too
    // hard to read the moment the frame rate is high.
    float smoothedFrameTime_ {1.0F / 60.0F};

    // How often the simulation steps, independent of how often a frame is drawn. Time the
    // frames deliver but the simulation has not consumed yet waits in the accumulator.
    float simulationRate_ {60.0F};
    float simulationAccumulator_ {0.0F};
    int   stepsLastFrame_ {0};

    // Time since the interface was last drawn. Only consulted while a benchmark is
    // recording, when frames are deliberately rare.
    float uiAccumulator_ {0.0F};
};

#endif
