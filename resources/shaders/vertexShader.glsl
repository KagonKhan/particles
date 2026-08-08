#version 430
in vec2 particlePos;

void main()
{
    gl_Position = vec4(particlePos, 0.0, 1.0);
    // gl_PointSize = 6.0; // otherwise points render at 1px, easy to miss
}