# particles

A 2D particle sandbox. Emitters spit out particles, attractors pull them around, shapes
bounce them off, and the whole thing gets drawn as points or splats. There's an ImGui panel
for poking at all of it while it runs.

Mostly an excuse to play with SIMD-ish data layout and see how many particles I can push
before the frame budget dies. Currently: about a million.

## Building

Conan + CMake. Roughly:

```sh
conan install . --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
```

Needs a C++26 compiler (GCC 14 works). TBB is optional-ish — without it the parallel
toggle just turns itself off.

## Layout

- `src/logic` — the simulation. Particle pool, scene objects (emitters, attractors,
  boundaries), behaviors, SDF shape stuff.
- `src/renderer` — OpenGL. A few pipelines depending on how you want particles drawn.
- `src/app` — window, ImGui layer, all the UI panels.
- `src/utils` — logging, RNG, shader cache, benchmarking, the usual junk drawer.
- `resources/shaders` — GLSL. `sdf.inl` is shared with the C++ side.

## Notes

`docs/performance.md` has the writeup of what was optimized and what was actually measured,
if you care. Read that before "improving" anything in the hot loop.
