#version 440

// No vertex buffer: the quad's four corners come out of gl_VertexID, so a sphere is
// fully described by the two uniforms below and nothing has to be uploaded per object.
uniform mat4  viewProj;
uniform vec2  center; // world space, flat in z like everything else in the scene
uniform float radius;

// World-space camera basis. Building the quad from it is what makes the impostor face
// the camera, and it costs nothing under either projection.
uniform vec3 camRight;
uniform vec3 camUp;

out vec2 quadCoord; // [-1,1] across the impostor, and the sphere's own x/y

void main()
{
    // Triangle strip corner order: (-1,-1) (1,-1) (-1,1) (1,1)
    quadCoord = vec2(((gl_VertexID & 1) == 0)? -1.0 : 1.0,
                     ((gl_VertexID & 2) == 0)? -1.0 : 1.0);

    // The body's centre lies in the simulation plane; the quad itself still faces the
    // camera, so the impostor reads as a ball sitting on the plane rather than a decal.
    vec3 world  = vec3(center, 0.0) + (radius * ((quadCoord.x * camRight) + (quadCoord.y * camUp)));
    gl_Position = viewProj * vec4(world, 1.0);
}
