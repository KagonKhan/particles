// The distance functions the simulation and shape.frag both evaluate, written once in the
// subset that is valid GLSL and valid C++ at the same time. logic/sdf.hpp includes this
// behind a `using namespace glm`, whose names and semantics are GLSL's by design; shape.frag
// includes it through the resolver in utils/shader_cache.cpp.
//
// Rules for anything added here: only the vector types and builtins glm also provides, and
// every float literal suffixed `f`, since a bare `0.0` is a double in C++ and -Wconversion
// says so. Anything that cannot be said that way belongs on one side or the other. In
// particular nothing here may branch on which shape it is: the two sides disagree about what
// a shape even is — a variant alternative on one, a tag in a vertex attribute on the other —
// and reconciling that is the caller's job, not this file's.

// `inline` in C++, where these definitions land in every translation unit that includes
// them. Nothing in GLSL, which has no such keyword and reserves the word.
#ifndef SDF
    #define SDF
#endif

// === OUTLINES ========================================================================================================
// Signed distance to the outline in the shape's own frame, negative inside.

SDF float sdfCircle(vec2 p, float radius)
{
    return length(p) - radius;
}

SDF float sdfBox(vec2 p, vec2 halfExtents)
{
    vec2 outside = abs(p) - halfExtents;
    return length(max(outside, 0.0f)) + min(max(outside.x, outside.y), 0.0f);
}

SDF float sdfSegment(vec2 p, float halfLength, float halfThickness)
{
    p.x -= clamp(p.x, -halfLength, halfLength);
    return length(p) - halfThickness;
}

// === FILLETS =========================================================================================================
// The largest ball that rolls along the inside of an outline without changing it, and so how
// much a body built on that outline may have its edges rounded by. One-liners, and shared
// anyway: a fillet that disagrees with the outline it belongs to draws a body a little away
// from where it collides, which is a hard thing to see and a harder one to attribute.

SDF float sdfCircleFillet(float radius)         { return radius; }
SDF float sdfSegmentFillet(float halfThickness) { return halfThickness; }
SDF float sdfBoxFillet()                        { return 0.0f; } // a box keeps its corners

// === EXTRUSION =======================================================================================================
// The solid an outline describes: swept `height` either side of its own plane with the edges
// rounded off by `fillet`, first taken back to what the height can hold. Exact, so a ray
// march over it can take full steps.

SDF float sdfExtrude(float outline, float z, float height, float fillet)
{
    float rounding = min(fillet, height);

    vec2 profile = vec2(outline + rounding, abs(z) - (height - rounding));

    return min(max(profile.x, profile.y), 0.0f) + length(max(profile, 0.0f)) - rounding;
}
