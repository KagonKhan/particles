cmake_minimum_required (VERSION 3.23)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()



# === SETUP ============================================================================================================
project (ImguiProject CXX)
set(PROJECT particles)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)



# === DEPENDENCIES =====================================================================================================
add_subdirectory(bindings)

find_package(stb REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(nlohmann_json REQUIRED)



# === PROJECT ==========================================================================================================
add_executable(${PROJECT}
    src/main.cpp

    src/app/app.cpp
    src/app/console.cpp

    src/emitter/emitter.cpp

    src/renderer/image.cpp
    src/renderer/renderer.cpp
    src/renderer/particle_buffer.cpp
    src/renderer/point_pipeline.cpp
    src/renderer/splat_pipeline.cpp
    src/renderer/sphere_pipeline.cpp

    src/utils/logger.cpp
    src/utils/shader_cache.cpp
    src/utils/rng.cpp
    src/app/scene.cpp
    src/logic/camera.cpp
    src/logic/objects/attractor.hpp
    src/logic/knob.hpp
)

set_property(TARGET ${PROJECT} PROPERTY CXX_STANDARD 26)


target_include_directories(${PROJECT} PUBLIC src)

target_link_libraries(${PROJECT} PUBLIC
    ImguiBindings
    
    stb::stb
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    none
)

target_compile_definitions(${PROJECT} PUBLIC RESOURCE_DIR=${PROJECT_SOURCE_DIR}/resources)
