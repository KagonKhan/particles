#version 430

uniform float time;
uniform float hueSpeed;
uniform float saturation; // reused as "mix towards white" amount, see below
uniform float brightness;

out vec4 fragColor;

void main()
{
    float t = time * hueSpeed;

    // Cosine-based rainbow palette (Inigo Quilez's palette trick).
    // Each channel is a phase-shifted cosine wave -> smooth, cheap rainbow cycling.
    vec3 color = 0.5 + 0.5 * cos(6.28318 * (t + vec3(0.0, 0.33, 0.67)));

    color = mix(vec3(1.0), color, saturation); // pull toward white as saturation -> 0
    color *= brightness;

    fragColor = vec4(color, 1.0);
}