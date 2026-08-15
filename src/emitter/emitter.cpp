#include "emitter.hpp"

#include <imgui.h>
#include <algorithm>

void Emitter::spawn(ParticlePool& pool, float dt)
{
    if (pool.aliveCount >= emittingSettings_.maxParticles) {
        return;
    }

    spawnAccumulator_ += emittingSettings_.spawnRate * dt;
    // Against the user's cap, not the pool's: clamping to MAX_PARTICLES let a single frame
    // overshoot whatever limit had been dialled in.
    int toSpawn = std::min(
        static_cast<int>(emittingSettings_.maxParticles) - static_cast<int>(pool.aliveCount),
        static_cast<int>(spawnAccumulator_));
    spawnAccumulator_ -= toSpawn;

    constexpr float kJitter = 0.1F;

    for (int i = 0; i < toSpawn; ++i) {
        glm::vec2 jitter {rng_.range(-kJitter, kJitter), rng_.range(-kJitter, kJitter)};

        // A direction on the unit circle rather than the unit sphere: a burst that fans out
        // evenly in the plane, with nothing left over to push a particle off it.
        glm::vec2 direction {};
        rng_.unitVector(direction.x, direction.y);

        pool.positions[pool.aliveCount]  = object_.transform.position + jitter;
        pool.velocities[pool.aliveCount] = direction * rng_.range(0.1F, 1.5F);
        pool.ages[pool.aliveCount]       = 0;

        ++pool.aliveCount;
    }
}

// Kept out of the scene's fused pass and given one of its own: it is the pool's shape that
// changes here, not the particles, and a chunk cannot pull a replacement in from a tail that
// belongs to some other chunk. It reads the ages, which the fused pass never touches, and
// writes only where something has actually died.
void Emitter::reap(ParticlePool& pool, float dt)
{
    std::size_t i = 0;

    while (i < pool.aliveCount) {
        pool.ages[i] += dt;
        if (pool.ages[i] >= emittingSettings_.maxAge) {
            --pool.aliveCount;

            pool.positions[i]  = pool.positions[pool.aliveCount];
            pool.velocities[i] = pool.velocities[pool.aliveCount];
            pool.ages[i]       = pool.ages[pool.aliveCount];
        }
        else {
            ++i;
        }
    }
}

void Emitter::renderSettings(ParticlePool& pool)
{
    ImGui::Begin("Emitter Settings");

    ImGui::Text("Active particles: %zu", pool.aliveCount);
    ImGui::Checkbox("Update", &emittingSettings_.enabled);
    ImGui::SliderFloat("Spawn rate", &emittingSettings_.spawnRate, 0.0F, 10'000, "%.0f /sec");
    ImGui::SliderFloat("Lifetime", &emittingSettings_.maxAge, 0.1F, 100.0F, "%.2f s");
    static std::size_t minParticles = 100;
    static std::size_t maxParticles = 1'000'000;

    ImGui::SliderScalar(
        "Maximum particles",
        ImGuiDataType_U64,
        &emittingSettings_.maxParticles,
        &minParticles,
        &maxParticles
    );
    if (ImGui::Button("Kill")) {
        pool.aliveCount = 0;
    }

    ImGui::SeparatorText("Body");

    renderSceneObjectSettings(object_);

    ImGui::End();
}
