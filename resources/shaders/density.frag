#version 440

in vec2 uv;
out vec4 fragColor;

uniform usampler2D densityImage;
uniform usampler2D hueImage;
uniform vec4 particleColor;
uniform float fadeScale; // controls how quickly density saturates to full color

// Cosine gradient: color = a + b * cos(2pi * (c*t + d)).
uniform vec3 paletteA;
uniform vec3 paletteB;
uniform vec3 paletteC;
uniform vec3 paletteD;
uniform float colorMix;   // 0 = flat particleColor, 1 = full gradient
uniform float coreWhiten; // how far the densest cores blow out toward white

// Fixed-point scale the splat pass accumulates with — must match splat.comp.
const uint kDensityScale = 64u;

void main()
{
    uint raw = texture(densityImage, uv).r;

    if (raw == 0u) {
        discard; // empty pixel, nothing to draw — cheap, only runs once per pixel anyway
    }

    // Both sums carry the same fixed-point scale, so the divide yields a plain
    // density-weighted average hue in [0,1).
    float hue = float(texture(hueImage, uv).r) / float(raw);

    float count = float(raw) / float(kDensityScale);

    // Exponential falloff: dense clusters glow toward full opacity,
    // sparse areas stay faint. Tune fadeScale to taste.
    float intensity = 1.0 - exp(-count * fadeScale);

    vec3 nebula = paletteA + paletteB * cos(6.28318530718 * (paletteC * hue + paletteD));
    vec3 rgb    = mix(particleColor.rgb, nebula, colorMix);

    // Real nebula imagery blows out to white where the gas is thickest. Biasing on
    // intensity^4 keeps the shift confined to the very brightest cores.
    rgb = mix(rgb, vec3(1.0), coreWhiten * pow(intensity, 4.0));

    fragColor = vec4(rgb, particleColor.a * intensity);
}
