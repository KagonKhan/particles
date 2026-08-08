#version 440

uniform vec4 particleColor;
out vec4 fragColor;

void main()
{
    vec2 d = gl_PointCoord * 2.0 - 1.0; // [-1,1] across the point sprite
    float dist = length(d);
    if (dist > 1.0) discard;
    fragColor = vec4(particleColor.rgb, 1.0);
}