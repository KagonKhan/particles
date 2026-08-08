#version 440

in vec2 particlePos; // location 0 now, one per vertex, no divisor needed

uniform float pointSizePixels;

void main()
{
    gl_Position = vec4(particlePos, 0.0, 1.0);
    gl_PointSize = pointSizePixels; // GPU handles pixel-space sizing natively
}