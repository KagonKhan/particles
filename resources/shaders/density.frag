#version 440

in vec2 uv;
out vec4 fragColor;

uniform usampler2D densityImage;
uniform vec4 particleColor;
uniform float fadeScale; // controls how quickly density saturates to full color

// Fixed-point scale the splat pass accumulates with — must match splat.comp.
const uint kDensityScale = 64u;

void main()
{
    uint raw = texture(densityImage, uv).r;

    if (raw == 0u) {
        discard; // empty pixel, nothing to draw — cheap, only runs once per pixel anyway
    }

    float count = float(raw) / float(kDensityScale);

    // Exponential falloff: dense clusters glow toward full opacity,
    // sparse areas stay faint. Tune fadeScale to taste.
    float intensity = 1.0 - exp(-count * fadeScale);

    fragColor = vec4(particleColor.rgb, particleColor.a * intensity);
}
