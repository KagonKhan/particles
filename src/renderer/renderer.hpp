#ifndef TEMPLATE_RENDERER_RENDERER_HPP
#define TEMPLATE_RENDERER_RENDERER_HPP

#include "image.hpp"

class Renderer
{
public:
    Renderer();

    Image          image {{800, 600}};
    unsigned char* data;

    void render();

private:
    bool generateNoise_ = false;
};

#endif // TEMPLATE_RENDERER_RENDERER_HPP
