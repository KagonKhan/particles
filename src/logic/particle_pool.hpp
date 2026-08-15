#ifndef YARR_LOGIC_PARTICLE_POOL_HPP
#define YARR_LOGIC_PARTICLE_POOL_HPP

#include <glm/ext/vector_float2.hpp>

#include <array>
#include <cstddef>
#include <span>


static constexpr std::size_t MAX_PARTICLES = 1'000'000;


// The simulation is flat: a particle has a position and a velocity in the plane, and
// nothing here can take it out of that plane. The camera is free to look at the plane
// from wherever it likes — that is the renderer's business, not the simulation's.
using ParticleVector = glm::vec2;

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
};

#endif // YARR_LOGIC_PARTICLE_POOL_HPP
