#ifndef YARR_RENDERER_HPP
#define YARR_RENDERER_HPP

#include "emitter/emitter.hpp"
#include <GL/glew.h> // or whatever GL loader you use
#include <GLFW/glfw3.h>

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void render(GLFWwindow* window);

private:
    void resizeDensityTexture(int w, int h);

    GLint pointSizeLoc_ {-1}; // kept for compatibility, unused now
    GLint colorLoc_ {-1};
    GLint splatColorLoc_ {-1};
    GLint particleCountLoc_ {-1};
    GLint screenSizeLoc_ {-1};
    GLint densitySamplerLoc_ {-1};
    GLint fadeLoc_ {-1};

    int texW_ {0};
    int texH_ {0};

    Emitter emitter;
};

#endif // YARR_RENDERER_HPP
