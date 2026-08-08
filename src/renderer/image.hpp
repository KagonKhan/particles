#ifndef YARR_IMAGE_HPP
#define YARR_IMAGE_HPP

#include "imgui.h"

#include <GLFW/glfw3.h>

class Image
{
public:
    explicit Image(ImVec2 size);

    void update(const void* data);
    void resize(ImVec2 new_size) { size_ = new_size; }

    GLuint id() const noexcept   { return textureID_; }
    ImVec2 size() const noexcept { return size_; }

private:
    GLuint textureID_;
    ImVec2 size_;
};

#endif
