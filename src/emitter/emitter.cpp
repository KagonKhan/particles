#include "emitter.hpp"

#include <imgui.h>
#include <algorithm>

void Emitter::spawn(float dt)
{
    if (pool_.aliveCount >= emittingSettings_.maxParticles) {
        return;
    }

    spawnAccumulator_ += emittingSettings_.spawnRate * dt;
    // Against the user's cap, not the pool's: clamping to MAX_PARTICLES let a single frame
    // overshoot whatever limit had been dialled in.
    int toSpawn = std::min(
        static_cast<int>(emittingSettings_.maxParticles) - static_cast<int>(pool_.aliveCount),
        static_cast<int>(spawnAccumulator_));
    spawnAccumulator_ -= toSpawn;

    constexpr float kJitter = 0.1F;

    for (int i = 0; i < toSpawn; ++i) {
        glm::vec2 jitter {rng_.range(-kJitter, kJitter), rng_.range(-kJitter, kJitter)};

        // A direction on the unit circle rather than the unit sphere: a burst that fans out
        // evenly in the plane, with nothing left over to push a particle off it.
        glm::vec2 direction {};
        rng_.unitVector(direction.x, direction.y);

        pool_.positions[pool_.aliveCount]  = object_.transform.position + jitter;
        pool_.velocities[pool_.aliveCount] = direction * rng_.range(0.1F, 1.5F);
        pool_.ages[pool_.aliveCount]       = 0;

        ++pool_.aliveCount;
    }
}

void Emitter::update(float dt)
{
    for (std::size_t i {0}; i < pool_.aliveCount; ++i) {
        pool_.positions[i] += pool_.velocities[i] * dt;
    }


    std::size_t i = 0;

    while (i < pool_.aliveCount) {
        pool_.ages[i] += dt;
        if (pool_.ages[i] >= emittingSettings_.maxAge) {
            --pool_.aliveCount;

            pool_.positions[i]  = pool_.positions[pool_.aliveCount];
            pool_.velocities[i] = pool_.velocities[pool_.aliveCount];
            pool_.ages[i]       = pool_.ages[pool_.aliveCount];
        }
        else {
            ++i;
        }
    }
}

void Emitter::renderSettings()
{
    ImGui::Begin("Emitter Settings");

    ImGui::Text("Active particles: %zu", pool_.aliveCount);
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
        pool_.aliveCount = 0;
    }

    ImGui::SeparatorText("Body");

    renderSceneObjectSettings(object_);

    ImGui::End();
}
