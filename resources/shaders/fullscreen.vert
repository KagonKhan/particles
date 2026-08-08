#version 440

// Classic "fullscreen triangle" — no vertex buffer needed.
// gl_VertexID 0,1,2 generate a triangle that covers the whole screen,
// clipped by the rasterizer, so the visible part is exactly the viewport.
out vec2 uv;

void main()
{
vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); // (0,0), (2,0), (0,2)
    uv = pos;                              // fixed: was pos * 0.5
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}