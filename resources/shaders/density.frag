#version 440

in vec2 uv;
out vec4 fragColor;

uniform usampler2D densityImage;
uniform vec4 particleColor;
uniform float fadeScale; // controls how quickly density saturates to full color

void main()
{
    uint count = texture(densityImage, uv).r;

    if (count == 0u) {
        discard; // empty pixel, nothing to draw — cheap, only runs once per pixel anyway
    }

    // Exponential falloff: dense clusters glow toward full opacity,
    // sparse areas stay faint. Tune fadeScale to taste.
    float intensity = 1.0 - exp(-float(count) * fadeScale);

    fragColor = vec4(particleColor.rgb, particleColor.a * intensity);
}