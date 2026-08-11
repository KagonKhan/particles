#version 440

layout(location = 0) in vec2 particlePos; // world space, one per point, flat in z

uniform mat4 viewProj;
uniform vec4 viewRowZ;        // third row of the view matrix
uniform float depthReference; // depth at which a point draws at exactly pointSize
uniform float pointSize;      // pixels
uniform bool attenuate;       // shrink with distance

void main()
{
    // The simulation is flat, so every particle sits at z = 0. The camera is not: it can
    // look at that plane from anywhere, which is why depth below is still worth computing.
    vec4 world = vec4(particlePos, 0.0, 1.0);

    gl_Position = viewProj * world;

    float size = pointSize;

    if (attenuate) {
        // Distance along the camera's forward axis, not euclidean distance to the eye:
        // the latter would shrink points toward the corners of a wide field of view.
        float depth = -dot(viewRowZ, world);
        size *= depthReference / max(depth, 1e-3);
    }

    // Drivers clamp gl_PointSize to their own range anyway, but keeping it off zero
    // stops a point behind the camera from vanishing into a degenerate sprite.
    gl_PointSize = clamp(size, 1.0, 256.0);
}
