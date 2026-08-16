 #define GLM_ENABLE_EXPERIMENTAL
#include "app/app.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <system_error>


/*
    THE TODO LIST:
    - Attractors (pull particles according to some math representation)
    - Boundaries (kill particles outside of bounds)
    - Keybinds / better IO / mouse
    - Scene / Scene editor
    - Saving settings to files
    - Scene saver
    - Multiple emitters and emitter types.



Every mature particle system (Cascade/Niagara, Shuriken, PopcornFX) converges on the same five-stage pipeline, with stacks of swappable modules at each stage rather than hardcoded behavior:


EMIT → INITIALIZE → SIMULATE (forces) → KILL → RENDER
Your TODO items are all instances of stages — "attractors" is one simulate module, "boundaries" is one kill module. The single highest-leverage architectural decision is deciding now whether those become if-branches in emitter.cpp:47-79 or a std::vector<std::unique_ptr<Module>> the scene owns. Everything below is cheap if you pick the latter and expensive if you don't.

What's missing, by stage
Emit — you have continuous rate only. Also want: bursts (N particles at time T, looping), distance-based emission (spawn per unit of emitter movement), and emitter shapes: point / sphere / box / disc / cone / line / mesh-surface. Your SpawnFrame planar flag is really a degenerate case of "disc shape".
Initialize — currently direction * linearRand(0.1, 1.5). Want per-particle: lifetime (yours is a single global maxAge; randomized per-particle lifetime is what breaks up the visible pulsing), mass/inverse-mass, size, color, rotation + angular velocity, and a stable per-particle seed so any later randomness is reproducible. Velocity modes: radial, cone-with-spread, along-shape-normal, inherit-emitter-velocity.
Simulate — this is your biggest gap. update() has no acceleration term at all, just pos += vel*dt. Once you add a force accumulator you get, nearly free: gravity, linear/quadratic drag, wind, point attractor/repeller (your TODO), vortex, curl noise (the single best-looking force for smoke/nebula work), turbulence, and velocity clamping. Beyond forces: collisions (plane / sphere / SDF, with restitution + friction), and neighbor-based forces (SPH, boids) if you ever want flocking — those need a spatial hash, which is also what your commented-out culled() was reaching for.
Curves over lifetime — size-over-life, alpha-over-life, color-over-life, drag-over-life. Not on your list and it should be near the top: it's the highest visual-quality-per-line-of-code feature in the entire domain. Your palette is keyed on spawn time, which is a different and complementary axis.
Kill — bounds (your TODO), but as a mode enum: kill / bounce / wrap / clamp. Plus kill-on-collide, kill-below-alpha-threshold.
Events / sub-emitters — spawn particles on death or on collision. This one feature is what gives you fireworks, sparks, and smoke trails, and it's why the emit stage should take a spawn source that isn't necessarily "an emitter".
Render — you have density splatting. Also: textured billboards, velocity-stretched billboards, ribbons/trails (huge visually, cheap to prototype), mesh instancing, soft particles against depth. Plus the blending/sorting question — additive needs no sort, alpha-blended sprites need back-to-front. And post: bloom is essentially mandatory for this aesthetic, plus HDR + a real tonemap (ACES/Reinhard) instead of coreWhiten faking the blowout.


Missing infrastructure, not features
Time control: pause, single-step, global time scale, and prewarm (simulate N seconds at startup so the system looks settled at t=0 instead of erupting from nothing). Prewarm is always forgotten and always wanted.
Fixed timestep with substepping. You integrate with raw frame dt; a hitching frame teleports every particle and desyncs spawnAccumulator_.
Determinism: a seed per emitter, saved with the scene. Without it "saving settings to files" doesn't reproduce a look.
Profiling HUD: frame graph, particle count, spawn/kill rates, ms per pipeline stage, GPU timer queries. You're targeting extreme counts — you'll be flying blind otherwise.
Two ImGui widgets you'll build once and use everywhere: a curve editor and a gradient editor. Budget for them; they're the actual cost of "curves over lifetime".
Presets library + hot-reload, which is what makes your scene-saver worth having.
Capture: screenshot and frame-sequence export.
Suggested ordering
Force accumulator + module list (unblocks attractors, gravity, drag, curl noise all at once)
Per-particle lifetime, size, color + curves-over-life
Bounds with mode enum
Emitter shapes + burst emission
Serialization (once the data model has stopped moving — doing it earlier means rewriting it)
Multiple emitters + scene tree UI + gizmos/mouse picking
Trails, sub-emitters, collisions
GPU compute simulation, if CPU becomes the wall
One thing worth looking at now
emitter.hpp:16 sets MAX_PARTICLES = 100'000'000, and ParticlePool holds three std::arrays inline: 1.2 GB + 1.2 GB + 400 MB = ~2.8 GB per Emitter, value-initialized at construction. Renderer is heap-allocated so it doesn't blow the stack, but Scene also holds an Emitter by value — if you ever wire Scene into App as a value member, that's a second 2.8 GB and a stack overflow. Making the pool a runtime-sized heap allocation with a capacity setting is a prerequisite for multiple emitters anyway.
*/


void preferHardwareRenderer()
{
#ifdef __linux__
    std::error_code error;

    if (std::filesystem::exists("/dev/dxg", error)) {
        setenv("GALLIUM_DRIVER", "d3d12", 0);
    }

#endif
}

int main()
{
    spdlog::set_level(spdlog::level::trace);

    preferHardwareRenderer();

    App app {"Template Project"};
    app.run();
}
