#ifndef TEMPLATE_RENDERER_RENDERER_HPP
#define TEMPLATE_RENDERER_RENDERER_HPP

#include "utils/opengl.hpp"

#include "emitter/emitter.hpp"
#include "image.hpp"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void render(GLFWwindow* window);

private:

    Emitter emitter;
    GLint   pointSizeLoc_   = -1;
    GLint   aspectScaleLoc_ = -1;
    GLint   colorLoc_       = -1;
};

#endif // TEMPLATE_RENDERER_RENDERER_HPP
