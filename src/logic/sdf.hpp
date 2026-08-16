#ifndef YARR_LOGIC_SDF_HPP
#define YARR_LOGIC_SDF_HPP

#include <glm/common.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>

// resources/shaders/sdf.inl, as C++. glm exists to mimic GLSL, so the whole shim is a using
// directive: vec2, length, abs, min, max and clamp all mean inside this namespace exactly
// what they mean in a shader, and the shared file needs no dialect of its own to be read
// both ways. Confined to the namespace, so nothing outside it sees glm's names unqualified.
namespace glsl
{

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace glm;

#define SDF inline
#include "sdf.inl"
#undef SDF

} // namespace glsl

#endif // YARR_LOGIC_SDF_HPP
