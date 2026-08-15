#ifndef YARR_LOGIC_PARTICLE_POOL_HPP
#define YARR_LOGIC_PARTICLE_POOL_HPP

#include <glm/ext/vector_float2.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>


static constexpr std::size_t MAX_PARTICLES = 1'000'000;

// How much of the pool is handed to the scene's objects at a time. A chunk's positions and
// velocities come to 16 KiB, so once it has been read in it stays in L1 for however many
// objects have something to say about it — which is the point of slicing the pool at all.
// The pool whole is tens of megabytes and would be evicted between every pair of them.
static constexpr std::size_t kChunkParticles = 1024;


// The simulation is flat: a particle has a position and a velocity in the plane, and
// nothing here can take it out of that plane. The camera is free to look at the plane
// from wherever it likes — that is the renderer's business, not the simulation's.
using ParticleVector = glm::vec2;

// A slice of live particles, and all an object acting on them is given: it may move them and
// change how they are moving, and it may not spawn, kill, or reorder — those change the
// pool's shape and belong to whoever owns it.
//
// Held by value. The spans are two pointers and a length, and copying them per chunk keeps
// the object's inner loop free of any indirection back to the pool.
struct ParticleChunk
{
    std::span<ParticleVector> positions;
    std::span<ParticleVector> velocities;

    [[nodiscard]] std::size_t size() const noexcept { return positions.size(); }
};

// The scene owns this. An emitter writes into it, every other scene object reads and
// rewrites it in place, and the renderer uploads it — none of them own it, so a particle
// outlives whatever spawned it.
struct ParticlePool
{
    std::array<ParticleVector, MAX_PARTICLES> positions {};
    std::array<ParticleVector, MAX_PARTICLES> velocities {};
    std::array<float, MAX_PARTICLES> ages {};

    std::size_t aliveCount {};

    [[nodiscard]] std::span<ParticleVector> alivePositions() noexcept
    {
        return {positions.data(), aliveCount};
    }

    [[nodiscard]] std::span<ParticleVector> aliveVelocities() noexcept
    {
        return {velocities.data(), aliveCount};
    }

    // The chunk starting at `first`, short at the end of the pool. Walked as
    // `for (first = 0; first < aliveCount; first += kChunkParticles)`.
    [[nodiscard]] ParticleChunk chunk(std::size_t first) noexcept
    {
        std::size_t const count = std::min(kChunkParticles, aliveCount - first);

        return {
            .positions  = alivePositions().subspan(first, count),
            .velocities = aliveVelocities().subspan(first, count),
        };
    }
};

#endif // YARR_LOGIC_PARTICLE_POOL_HPP
