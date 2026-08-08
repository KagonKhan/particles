#ifndef TEMPLATE_RENDERER_RENDERER_HPP
#define TEMPLATE_RENDERER_RENDERER_HPP

#include "utils/opengl.hpp"

#include "emitter/emitter.hpp"
#include "image.hpp"

class Renderer
{
public:
    Renderer();

    void render(GLFWwindow* window);

private:

    Emitter emitter;
};

#endif // TEMPLATE_RENDERER_RENDERER_HPP
