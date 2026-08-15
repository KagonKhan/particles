# Performance: what was changed, what was measured, what is still open

Written 2026-08-15. Everything here was measured on this machine — a Ryzen 9 7950X3D under
WSL2, GCC 14, Release (`-O3`) — at 1,000,000 particles in steady state. Numbers are medians
over 2000+ simulation steps unless stated otherwise.

Two independent runs of the same configuration landed 0.15% apart (2988.7 vs 2993.3 µs), so
**differences above roughly 1% are signal and anything below is not**.

---

## 1. What changed in the code

### Broad phase before the contact test

`Bounce` used to build a full `Contact` — signed distance *and* gradient, two square roots —
for every particle against every body, dispatched through `std::visit` per particle.

Now `mayContact(shape, point, margin)` in `shape.hpp` rejects with a handful of compares and
no square root. One overload per shape; each is conservative in the one safe direction, so it
returns false only when the contact distance is certainly at least the margin.

| shape | test | |
|---|---|---|
| Circle | `dot(p,p) < (r+skin)²` | exact |
| Box | `max(\|p\|-h) < skin` | conservative in corner regions |
| Segment | same, on the enclosing box | capsule ⊆ box, so the box never overstates |
| HalfPlane | `p.y < skin` | exact |
| Frame | `max(\|p\|-h) > -skin` | exact, inverted as the contact is |

There is deliberately no `Shape` overload: reached through the variant it would cost more than
the test saves.

### Chunked loop fusion

Each scene object used to own a loop over the whole pool. The pool is ~20 MB, so every object
paid to pull the same particles back in, and cost grew with the *scene* rather than with the
physics.

`Scene::update` now owns one loop. The pool is sliced into `kChunkParticles` chunks and each
slice is given to every object in turn:

```cpp
for (std::size_t first = 0; first < pool_->aliveCount; first += chunkParticles_) {
    chunks_.push_back(pool_->chunk(first, chunkParticles_));
}
// attractor_->apply(chunk, dt);  integrate(chunk, dt);  boundary_->apply(chunk);
```

Fusion is **chunked, not per-particle**, and that distinction is the whole design. Per-particle
fusion would put the object list inside the inner loop and cost a virtual call and a variant
visit per particle per object — more than the memory traffic it saves. Per chunk, dispatch
happens ~1000 times a frame instead of a million, the behavior list stays dynamic, and each
kernel remains a tight statically-dispatched loop over contiguous spans.

Consequences worth remembering:

- `Behavior::update(pool, object)` became `apply(chunk, object)`.
- Integration moved out of `Emitter` into `scene.cpp`. It is the simulation's integrator and
  has to sit *between* the forces and the surfaces, which is only expressible once the scene
  owns the loop.
- `Emitter::update` became `Emitter::reap` — aging and removal only. It stays a separate pass
  because it changes the pool's *shape*: swap-remove pulls a replacement from a tail belonging
  to some other chunk. It reads `ages`, which the fused pass never touches.
- Chunks are independent by construction, which is what makes the parallel path safe.

---

## 2. Measurements

### Serial vs parallel, chunk 1024

| config | median | p95 | sd/med | speedup |
|---|---:|---:|---:|---:|
| seq, 32 vCPU | 2993.3 µs | 3124.7 | 3.0% | — |
| seq, 16 vCPU | 2959.7 µs | 3086.7 | 2.4% | — |
| seq, 16 vCPU + pinned CCD0 | 3102.9 µs | 3194.5 | 1.5% | — |
| par, 32 vCPU | 384.7 µs | 520.4 | 17.6% | 7.78x |
| **par, 16 vCPU** | **372.2 µs** | **460.5** | **11.8%** | **7.95x** |
| par, 16 vCPU + pinned CCD0 | 444.3 µs | 491.6 | 11.1% | 6.98x |

Best configuration measured: **16 vCPUs, unpinned, parallel, chunk 1024 → 372.2 µs**, which is
8.03x the original serial baseline. Effective traffic (32 MB moved per pass) goes from
10.7 GB/s single-threaded to 86.0 GB/s.

### Chunk size sweep, 32 vCPU

| chunk | KiB | chunks | serial | parallel |
|---:|---:|---:|---:|---:|
| 91 | 1 | 10990 | — | 421.3 µs |
| 304 | 4 | 3290 | — | 392.1 µs |
| 535 | 8 | 1870 | — | 394.8 µs |
| **1024** | **16** | **977** | **2993.3 µs** | **384.7 µs** |
| 1311 | 20 | 763 | — | 388.9 µs |
| 1438 | 22 | 696 | — | 401.9 µs † |
| 1618 | 25 | 619 | — | 385.1 µs |
| 1797 | 28 | 557 | 3109.6 µs | — |
| 2013 | 31 | 497 | 3124.4 µs | — |
| 65536 | 1024 | 16 | 3184.4 µs | 595.6 µs |

† drifted +9.1% within the run against ±3% everywhere else — treat as contaminated.

**`kChunkParticles = 1024` is validated**: best in both modes.

Serial shows the L1d story cleanly — cost rises monotonically as the chunk approaches and
exceeds the 32 KiB L1d (+3.9% at 28 KiB, +4.4% at 31 KiB, +6.4% at 1 MiB). The effect is small
but consistent and above the noise floor.

Parallel is a **plateau, not a peak**: everything from 304 to 1618 sits within 5%. The two
edges fail for different reasons, and only one of them is about cache:

- **91 particles/chunk** (+9.5%) — 10,990 chunks, per-chunk overhead dominates.
- **65536 particles/chunk** (+55%) — only **16 chunks for 32 threads**, so half the cores idle.
  A load-balance failure, not a cache failure, which is why the parallel penalty (55%) dwarfs
  the serial one (6.4%).

The practical guard this implies — keep the chunk count at roughly 4x the thread count — is
**not implemented**. See open items.

---

## 3. Two silent fallbacks found

Both had the same shape: the thing works, produces no diagnostic, and does nothing.

### `std::execution::par` was running serially

libstdc++ selects its PSTL backend from `__has_include(<tbb/tbb.h>)` at the point `<execution>`
is parsed (see `bits/c++config.h`). Without TBB headers on the include path it defines
`_PSTL_PAR_BACKEND_SERIAL` — and then compiles, links, runs, and ignores every execution policy.

Measured before adding TBB: **par 2603 ms vs seq 2613 ms** on 32 threads. After: **112 ms vs
2879 ms, 25.7x**.

`onetbb/2021.12.0` is now a conan dependency (it needs `hwloc/*:shared=True`), CMake does
`find_package(TBB QUIET)`, and `scene.cpp` turns the failure into a build error rather than
trusting the link line:

```cpp
#if defined(__GLIBCXX__) && !defined(_PSTL_PAR_BACKEND_TBB)
    #error "TBB is linked but <execution> selected its serial backend"
#endif
```

### OpenGL was running on the CPU

Mesa's driver auto-selection on this system picks **llvmpipe**, its software rasterizer, in
preference to the WSLg hardware path. Nothing about the app provokes it — an early guess that
the `GL 4.4 core` request was too much for the `d3d12` driver was **wrong**. Probed directly at
every version from 3.3 to 4.6:

```
default                 4.4 core -> llvmpipe (LLVM 20.1.2)      | GL 4.5 Mesa 25.2.8
LD_LIBRARY_PATH=/usr/lib/wsl/lib  4.4 core -> llvmpipe          | GL 4.5
GALLIUM_DRIVER=d3d12    4.4 core -> D3D12 (AMD Radeon RX 6800 XT) | GL 4.6 Mesa 25.2.8
```

The hardware driver serves **GL 4.6**, comfortably above what the app asks for, and setting the
WSLg library path alone changes nothing. `GALLIUM_DRIVER=d3d12` is the necessary and sufficient
switch.

`preferHardwareRenderer()` in `main.cpp` now sets it, before anything touches GL. In the process
rather than in the build, so it holds however the binary was started — shell, IDE, debugger,
launcher script — and guarded on `/dev/dxg` existing, so it cannot name a driver that is absent
on a machine where GL was working. It never overwrites, so `GALLIUM_DRIVER=llvmpipe` still
forces software when that is what you want. `App`'s constructor logs the renderer that answered
and warns if it is llvmpipe.

Beware that the GPU showing activity is not evidence the app is using it: WSLg composites and
presents the window through the GPU regardless, so a software-rendered app still moves the GPU
meter. The reliable test is the thread list — `llvmpipe-N` threads inside the process mean
software rasterization, and they do not exist under `d3d12`.

Measured from inside the guest with the app running, over 4 s:

```
per-vCPU: 56-61% busy, uniformly across all 16
  51.0% of one core   particles (main thread)
  ~21%  of one core   llvmpipe-0 .. llvmpipe-15   (16 threads, one per vCPU)
  process total: 884% of one core = 55.3% of 16 vCPUs
```

For scale: the fused pass at 372 µs × 60 steps/s is 22 ms of wall time per second, about 36% of
one core. **The simulation was roughly 4% of the process's CPU. The other 96% was drawing.**

Measured again with `GALLIUM_DRIVER=d3d12`, same scene, same 4 s window:

| | llvmpipe | d3d12 |
|---|---:|---:|
| threads in process | 49 | 6 |
| process CPU | 884% of one core | **88.2%** |
| mean per-vCPU (of 16) | 58.3% | **7.5%** |

A tenfold reduction, and the load collapses onto the single main thread with fifteen vCPUs left
idle. Every measurement in section 2 predates this and needs re-taking.

---

## 4. WSL2 and the V-Cache die

**The guest cannot see the real topology.** WSL2 reports all CPUs sharing one 96 MiB L3 and one
die (`shared_cpu_list` = `0-31`, `die_cpus_list` = `0-31`). The hardware actually has 96 MiB on
CCD0 and 32 MiB on CCD1. L1d (32 KiB) and L2 (1 MiB) are reported correctly and are unaffected
by the V-Cache, which is L3 only.

`utils/topology.cpp` detects this — it groups CPUs by last-level-cache size, finds one group,
and disables the in-app pinning toggle with the reason stated rather than offering a no-op.

**Pinning is possible, but only from the Windows host.** WSL2 runs under the Hyper-V *root
scheduler*, which is the default and only supported configuration on Windows client since 10
v1803: "the hypervisor gives the root partition control of work scheduling. In the root
partition OS instance, the NT scheduler manages all aspects of assigning work to system logical
processors." Guest VPs are therefore ordinary NT-schedulable threads and ordinary affinity
applies. From an **elevated** PowerShell:

```powershell
$p = Get-Process vmmemWSL,vmmem -ErrorAction SilentlyContinue | Select-Object -First 1
$p.ProcessorAffinity = [IntPtr]0xFFFFL          # LPs 0-15  = CCD0 (V-Cache)
$p.ProcessorAffinity = [IntPtr]0xFFFF0000L      # LPs 16-31 = CCD1
'{0} pid={1} affinity=0x{2:X16}' -f $p.ProcessName, $p.Id, [int64]$p.ProcessorAffinity
```

Gotchas, all of which cost time once:

- **The process is `vmmem` on this build**, not `vmmemWSL`. Query both names. It carries 34
  threads against 32 logical CPUs — those are the VP dispatch threads. `vmwp` (97 threads) is
  device emulation and is the wrong target.
- **Requires elevation.** Unelevated, reading `ProcessorAffinity` returns `0x0` rather than
  throwing, which looks like a real answer.
- **Windows PowerShell 5.1 parses `0xFFFF0000` as Int32 = −65536.** Cast to `IntPtr` it
  sign-extends to `0xFFFFFFFFFFFF0000` — a request for CPUs 16–63 — and
  `SetProcessAffinityMask` fails with "The parameter is incorrect". Use the `L` suffix.
  `[int64]0xFFFF0000` does **not** help; the truncation happens at parse time. Print with
  `{0:X16}` so sign extension is visible.
- **`0xFFFF` is 8 physical cores, not 16** — LPs 0–15 are cores 0–7 with both SMT threads. For
  one thread per physical core use `0x5555` with `processors=8`.
- **Set `processors=16` in `.wslconfig` first, then `wsl --shutdown`, then pin.** Affinity does
  not survive the shutdown, and 32 vCPUs on 16 LPs is worse than not pinning.
- The `pinned` column in `summary.csv` reads **0 even for host-pinned runs** — the app's toggle
  is disabled under WSL, so host-side pinning exists only in the label you typed.

### What pinning actually showed

Pinning to CCD0 cost **19.4% parallel** and **4.8% serial**. Neither number is a clean test of
the V-Cache:

- The parallel figure is confounded — `0xFFFF` halved the *physical* core count from 16 to 8.
  That it only cost 19% rather than ~50% is itself evidence the loop is bandwidth-bound.
- The serial figure is the informative one. One thread does not care how many cores exist, yet
  it slowed 4.8% — close to the V-Cache die's known clock deficit (~5.25 vs ~5.75 GHz). Decent
  evidence that `0xFFFF` really is CCD0, and that at 1M particles it is a net loss: no cache
  benefit, because the ~20 MB working set fits in *either* die's L3, and all of the frequency
  penalty.
- Pinning did buy determinism: the pinned serial run has the tightest distribution measured
  (p95/median 1.030). Useful for low-noise A/B, not for throughput.

The honest conclusion is that **the V-Cache die is not expected to help below ~2M particles**,
where the pool stops fitting in CCD1's 32 MiB L3.

---

## 5. Results to distrust

- **"16 vCPUs beat 32 vCPUs" (372.2 vs 384.7 µs) is confounded.** llvmpipe sizes its thread
  pool to the core count, so at 32 vCPUs there were ~32 rasterizer threads competing with the
  TBB workers instead of 16. This may be renderer contention rather than SMT. Re-measure after
  the rendering problem is fixed before believing it.
- **All parallel numbers were taken under renderer contention.** 16 llvmpipe threads shared 16
  vCPUs with 16 TBB workers. This fits the variance signature exactly: parallel p95/median is
  1.24–1.35 while serial — one thread, easily accommodated — is 1.03–1.04.
- **Serial numbers and the chunk sweep are comparatively safe**: single-threaded, and every run
  was contaminated equally.
- The chunk-1438 run drifted +9.1% within itself; discard it.

---

## 6. Open items, in the order worth doing

1. ~~**Fix or bypass software rendering.**~~ Done, twice over. Launch with
   `GALLIUM_DRIVER=d3d12` to get the GPU, and a recording benchmark now skips the scene pass
   and drops the interface to 10 Hz regardless (`App::run`, gated on `Scene::benchmarking()`).
   The two are complementary: the first fixes the application, the second keeps a run honest
   even if it is ever launched without the variable.
2. **Re-measure everything on the GPU path.** Every number in section 2 was taken under
   software rendering. The 16-vs-32 vCPU result especially — see section 5.
3. **Guard the chunk count.** Clamp so chunks ≥ ~4x thread count; this would have caught the
   65536 case automatically. Warn in the tuning window when the setting starves threads.
4. **Record host-side conditions.** A free-text notes field written into `summary.csv`, so
   things the app cannot detect (host affinity, `.wslconfig`) are not lost to the label string.
5. **The real V-Cache A/B**: `0xFFFF` vs `0xFFFF0000`, same 8 physical cores either way, so the
   core-count confound cancels. Only worth doing after item 1, and more interesting above 2M
   particles.
6. **Bench cannot stop early and keep data** — `Cancel` discards. A "Stop & write" that calls
   `finish()` is a two-line change.

Beyond this, the architecture options considered but not taken, roughly in order of payoff:
multithreading the chunk loop with a hand-rolled static-partition pool (leaner than TBB's
work-stealing for uniform per-particle cost, and the only option that survives overlapping
simulation with rendering); moving the simulation to a GPU compute shader (the endgame — it
would also delete the SDF duplication between `shape.hpp` and `shape.frag`); component-wise SoA
(`posX[] / posY[]`) for SIMD; and a spatial grid, which is not worth it until particle–particle
interaction or dozens of bodies exist.
