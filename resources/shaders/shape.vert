#version 440

// One instance per body, geometry being a quad whose four corners come out of
// gl_VertexID. The whole scene is a single draw call however many shapes it holds.

layout(location = 0) in vec4 inOrigin; // xy world, z the shape type, w a third dimension
layout(location = 1) in vec4 inParams; // xy dimensions, z height, w bounding radius
layout(location = 2) in vec4 inColor;

uniform mat4 viewProj;
uniform vec3 camRight;
uniform vec3 camUp;

out vec3       quadWorld;
flat out vec2  bodyOrigin;
flat out uint  bodyType;
flat out float bodyWall;
flat out vec4  bodyParams;
flat out vec4  bodyColor;

void main()
{
    // Triangle strip corner order: (-1,-1) (1,-1) (-1,1) (1,1)
    vec2 corner = vec2(((gl_VertexID & 1) == 0)? -1.0 : 1.0,
                       ((gl_VertexID & 2) == 0)? -1.0 : 1.0);

    bodyOrigin = inOrigin.xy;
    bodyType   = uint(inOrigin.z + 0.5);
    bodyWall   = inOrigin.w;
    bodyParams = inParams;
    bodyColor  = inColor;

    // Screen facing and sized to the body's bounding sphere. shape.frag ray marches for
    // the surface, so this quad is only something to rasterize — facing the camera is what
    // keeps a body's silhouette inside it whatever angle the body is seen from.
    quadWorld = vec3(bodyOrigin, 0.0) + (inParams.w * ((corner.x * camRight) + (corner.y * camUp)));

    gl_Position = viewProj * vec4(quadWorld, 1.0);
}
