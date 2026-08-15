#version 440

// One instance per body, geometry being a quad whose four corners come out of
// gl_VertexID. The whole scene is a single draw call however many shapes it holds.

layout(location = 0) in vec4 inPlacement; // xy world centre, zw the footprint's half extents
layout(location = 1) in vec4 inParams;    // xy dimensions, z height, w the shape type
layout(location = 2) in vec4 inColor;

uniform mat4 viewProj;
uniform vec3 camRight;
uniform vec3 camUp;
uniform vec3 cameraEye;
uniform vec3 cameraForward;
uniform bool orthographic;

out vec3       quadWorld;
flat out vec2  bodyOrigin;
flat out vec3  bodyExtents;    // the box the body is confined to, so the march can stop
flat out vec2  bodyDimensions;
flat out uint  bodyType;
flat out vec4  bodyColor;

void main()
{
    // Triangle strip corner order: (-1,-1) (1,-1) (-1,1) (1,1)
    vec2 corner = vec2(((gl_VertexID & 1) == 0)? -1.0 : 1.0,
                       ((gl_VertexID & 2) == 0)? -1.0 : 1.0);

    bodyOrigin     = inPlacement.xy;
    bodyExtents    = vec3(inPlacement.zw, inParams.z);
    bodyDimensions = inParams.xy;
    bodyType       = uint(inParams.w + 0.5);
    bodyColor      = inColor;

    // Screen facing, since that is what keeps a body's silhouette inside the quad whatever
    // angle it is seen from, but sized to the body's box rather than to a square on the
    // sphere around it. A long thin body is most of a sphere's area and none of its volume,
    // and every fragment of that difference is a full march that was never going to hit.
    // A box measures along a unit axis as the dot with that axis made positive.
    vec3 centre   = vec3(bodyOrigin, 0.0);
    vec2 halfSize = vec2(dot(abs(camRight), bodyExtents), dot(abs(camUp), bodyExtents));

    // Which is the whole of it under a parallel projection. Under perspective the box's
    // near face projects larger than the plane through its centre the quad lies in, and a
    // body off to the side of the view projects further out again — the near face is not
    // only bigger but somewhere else. Both are the same correction: measure the box at the
    // centre, where its position is right, and widen it until it reaches what the near face
    // covers. The quad stays on the centre plane, so moving off axis cannot slide it off
    // the body the way carrying it forward would.
    if (!orthographic) {
        vec3  forward = normalize(cameraForward);
        vec3  toBody  = centre - cameraEye;

        float depth     = dot(toBody, forward);
        float halfDepth = dot(abs(forward), bodyExtents);
        vec2  offAxis   = vec2(dot(toBody, camRight), dot(toBody, camUp));

        // A camera level with the box has no near face to reach and no plane that could
        // bound it; the floor leaves the quad large rather than inside out.
        halfSize = ((depth * halfSize) + (abs(offAxis) * halfDepth)) / max(depth - halfDepth, 1e-3);
    }

    quadWorld   = centre + ((corner.x * halfSize.x) * camRight) + ((corner.y * halfSize.y) * camUp);
    gl_Position = viewProj * vec4(quadWorld, 1.0);
}
