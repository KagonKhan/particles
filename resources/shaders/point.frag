#version 440

uniform vec4 particleColor;
uniform bool roundPoints;
uniform float softness; // 0 = hard disc edge, 1 = the whole sprite is a falloff

out vec4 fragColor;

void main()
{
    float alpha = particleColor.a;

    if (roundPoints) {
        vec2  offset = (gl_PointCoord * 2.0) - 1.0; // [-1,1] across the sprite
        float dist   = length(offset);

        if (dist > 1.0) {
            discard; // outside the inscribed circle, so points read as round not square
        }

        // Fade the outer `softness` fraction of the radius. The caller keeps softness
        // off zero, which would collapse the smoothstep edges onto each other.
        alpha *= 1.0 - smoothstep(1.0 - softness, 1.0, dist);
    }

    fragColor = vec4(particleColor.rgb, alpha);
}
