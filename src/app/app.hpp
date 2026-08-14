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

    void run(int fps = -1);

private:
    void initializeGLFW(std::string const& window_name);
    void initializeIMGUI();

    void startNewFrame();
    void finishFrame();

    GLFWwindow*   window;
    Renderer*     renderer;
    Scene         scene;
    OutputConsole console;
};

#endif
