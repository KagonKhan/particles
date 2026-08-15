#version 440

// A body is its 2D outline extruded to `height` either side of its own plane, with the
// edges filleted. Height and fillet are separate because they have to be: a wall is tall
// and barely rounded, while a sphere is a circle whose fillet is its whole radius.
//
// The fillet a shape can take is the largest ball that rolls inside its outline without
// changing it — the radius for a circle, the half thickness for a capsule, and nothing at
// all for a box, whose corners are sharp by definition.

// A half plane is unbounded and a frame is a room, so shape_pipeline.cpp sends the first
// over as the box it draws as and the second as the four boxes its walls are. Neither has
// a case here.
const uint kCircle  = 0u;
const uint kBox     = 1u;
const uint kSegment = 2u;

const int kMaxSteps = 32;

in vec3       quadWorld;
flat in vec2  bodyOrigin;
flat in vec3  bodyExtents;    // the box the body is confined to, so the march can stop
flat in vec2  bodyDimensions;
flat in uint  bodyType;
flat in vec4  bodyColor;

uniform mat4  viewProj;
uniform vec3  cameraEye;     // where the view rays leave from, under perspective
uniform vec3  cameraForward; // the direction they all run in, under orthographic
uniform bool  orthographic;
uniform vec3  lightDir;      // world space, surface toward the light
uniform float ambient;

out vec4 fragColor;

float boxDistance(vec2 p, vec2 halfExtents)
{
    vec2 outside = abs(p) - halfExtents;
    return length(max(outside, 0.0)) + min(max(outside.x, outside.y), 0.0);
}

// Has to stay in step with logic/shape.hpp.
float outlineDistance(vec2 p)
{
    if (bodyType == kBox) {
        return boxDistance(p, bodyDimensions);
    }

    if (bodyType == kSegment) {
        p.x -= clamp(p.x, -bodyDimensions.x, bodyDimensions.x);
        return length(p) - bodyDimensions.y;
    }

    return length(p) - bodyDimensions.x;
}

float filletRadius()
{
    if (bodyType == kSegment) {
        return bodyDimensions.y;
    }

    if (bodyType == kCircle) {
        return bodyDimensions.x; // the sphere case, when it equals the height
    }

    return 0.0; // a box keeps its corners
}

// Exact, so the march below can take full steps.
float bodyDistance(vec3 world)
{
    float height = bodyExtents.z;
    float fillet = min(filletRadius(), height);

    vec2 profile = vec2(outlineDistance(world.xy - bodyOrigin) + fillet,
                        abs(world.z) - (height - fillet));

    return min(max(profile.x, profile.y), 0.0) + length(max(profile, 0.0)) - fillet;
}

vec3 bodyNormal(vec3 world, float epsilon)
{
    vec2 k = vec2(1.0, -1.0);

    return normalize((k.xyy * bodyDistance(world + (k.xyy * epsilon))) +
                     (k.yyx * bodyDistance(world + (k.yyx * epsilon))) +
                     (k.yxy * bodyDistance(world + (k.yxy * epsilon))) +
                     (k.xxx * bodyDistance(world + (k.xxx * epsilon))));
}

void main()
{
    vec3 origin = quadWorld;
    vec3 ray    = orthographic? normalize(cameraForward) : normalize(origin - cameraEye);

    // The stretch of the ray inside the body's box, which is the only stretch of it worth
    // marching. Solved rather than guessed at: the quad is square to the ray only at the
    // centre of the screen, so a window centred on the body would sit off to one side
    // everywhere else and shave the far edge off it.
    //
    // A ray parallel to an axis divides by zero there. Nudged off it, the slab comes back
    // enormous but finite, which the comparisons below handle and a NaN would not.
    vec3 direction = ray + (step(abs(ray), vec3(0.0)) * 1e-6);
    vec3 centred   = vec3(bodyOrigin, 0.0) - origin;

    vec3 lower = (centred - bodyExtents) / direction;
    vec3 upper = (centred + bodyExtents) / direction;

    vec3 entering = min(lower, upper);
    vec3 leaving  = max(lower, upper);

    // The quad sits on the plane through the body's centre, so this entry is usually behind
    // the fragment the ray was cast from and the march starts by walking back to it. Left
    // to start where it stands, a ray would find itself already inside the solid and report
    // a hit there — the body drawn as the flat section the quad cuts out of it.
    float entry = max(max(entering.x, entering.y), entering.z);
    float limit = min(min(leaving.x, leaving.y), leaving.z);

    if (entry > limit) {
        discard; // the ray misses the box, so it misses the body
    }

    float epsilon = max(max(bodyExtents.x, max(bodyExtents.y, bodyExtents.z)) * 1e-4, 1e-6);
    float travel  = entry;
    bool  hit     = false;

    for (int i = 0; i < kMaxSteps; ++i) {
        float remaining = bodyDistance(origin + (ray * travel));

        if (remaining < epsilon) {
            hit = true;
            break;
        }

        travel += remaining;

        if (travel > limit) {
            break;
        }
    }

    if (!hit) {
        discard;
    }

    vec3 surface = origin + (ray * travel);
    vec3 normal  = bodyNormal(surface, epsilon);

    // Lit in world space, so orbiting reads as moving around a solid body rather than
    // dragging a headlight over it.
    float diffuse = max(dot(normal, lightDir), 0.0);

    fragColor = vec4(bodyColor.rgb * (ambient + ((1.0 - ambient) * diffuse)), bodyColor.a);

    // Depth of the surface the ray hit, not of the quad it was found through, so bodies
    // that overlap on screen sort by which is actually nearer.
    vec4 clip    = viewProj * vec4(surface, 1.0);
    gl_FragDepth = ((clip.z / clip.w) * 0.5) + 0.5;
}
