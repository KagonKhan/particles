find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glew REQUIRED)


add_library(ImguiBindings
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_glfw.h

    imgui/imgui_impl_opengl3_loader.h

    imgui/imgui_impl_opengl3.cpp
    imgui/imgui_impl_opengl3.h
)

target_link_libraries(ImguiBindings imgui::imgui GLEW::GLEW glfw)

target_include_directories(ImguiBindings PUBLIC imgui/)
target_compile_definitions(ImguiBindings PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLEW)
target_compile_definitions(ImguiBindings PUBLIC IMGUI_DEFINE_MATH_OPERATORS)