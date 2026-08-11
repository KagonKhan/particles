#version 440

in vec2 quadCoord;

uniform vec4 color;
uniform vec3 camRight;
uniform vec3 camUp;
uniform vec3 lightDir; // world space, pointing from the surface toward the light

out vec4 fragColor;

void main()
{
    float r2 = dot(quadCoord, quadCoord);
    if (r2 > 1.0) {
        discard; // the quad's corners lie outside the sphere it stands in for
    }

    // The normal of the sphere this quad represents: the unit vector from its centre,
    // rebuilt in the camera's basis. cross(right, up) is the axis pointing back at the
    // viewer, which is where the third component goes.
    vec3 towardViewer = cross(camRight, camUp);
    vec3 normal       = (quadCoord.x * camRight) + (quadCoord.y * camUp) + (sqrt(1.0 - r2) * towardViewer);

    // Lit in world space rather than from the eye, so orbiting reads as moving around a
    // solid body instead of dragging a headlight over a flat disc.
    float diffuse = max(dot(normal, lightDir), 0.0);

    const float kAmbient = 0.15;
    fragColor = vec4(color.rgb * (kAmbient + ((1.0 - kAmbient) * diffuse)), color.a);
}
