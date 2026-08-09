#include "emitter.hpp"

#include <glm/gtc/random.hpp>
#include <algorithm>
#include <unordered_set>

void Emitter::spawn(glm::vec3 position, float dt)
{
    if (aliveCount_ >= MAX_PARTICLES) {
        return;
    }

    spawnAccumulator_ += settings_.spawnRate * dt;
    int toSpawn = std::min((int)MAX_PARTICLES - (int)aliveCount_, static_cast<int>(spawnAccumulator_));
    spawnAccumulator_ -= toSpawn;

    for (int i = 0; i < toSpawn; ++i) {
        glm::vec3 jitter {rng_.range(-0.1F, 0.1F), rng_.range(-0.1F, 0.1F), rng_.range(-0.1F, 0.1F)};

        pool_.positions[aliveCount_]   = position + jitter;
        pool_.velocities[aliveCount_]  = glm::sphericalRand(1.0F);
        pool_.velocities[aliveCount_] *= glm::linearRand(0.1F, 1.5F);
        pool_.ages[aliveCount_]        = 0;

        ++aliveCount_;
    }
}

void Emitter::update(float dt)
{
    if (!settings_.enabled) {
        return;
    }

    for (std::size_t i {0}; i < aliveCount_; ++i) {
        pool_.positions[i] += pool_.velocities[i] * dt;

        // The cloud now lives in a cube, so bound all three axes.
        // glm::vec3 const& p = pool_.positions[i];
        // if (glm::any(glm::greaterThan(glm::abs(p), glm::vec3(1.0F)))) {
        //     pool_.ages[i] = settings_.maxAge;
        // }
    }


    std::size_t i = 0;

    while (i < aliveCount_) {
        pool_.ages[i] += dt;
        if (pool_.ages[i] >= settings_.maxAge) {
            --aliveCount_;

            pool_.positions[i]  = pool_.positions[aliveCount_];
            pool_.velocities[i] = pool_.velocities[aliveCount_];
            pool_.ages[i]       = pool_.ages[aliveCount_];
        }
        else {
            ++i;
        }
    }
}

void Emitter::renderSettings()
{
    ImGui::Begin("Emitter Settings");

    ImGui::Text("Active particles: %zu", aliveCount_);
    ImGui::Checkbox("Update", &settings_.enabled);
    ImGui::SliderFloat("Spawn rate", &settings_.spawnRate, 0.0F, MAX_PARTICLES, "%.0f /sec");
    ImGui::SliderFloat("Lifetime", &settings_.maxAge, 0.1F, 100.0F, "%.2f s");

    if (ImGui::Button("Kill")) {
        aliveCount_ = 0;
    }

    ImGui::End();
}

// std::pair<ImVec2 const*, std::size_t> Emitter::culled(float cell_size)
// {
//     std::unordered_set<std::int64_t> occupiedCells;
//     occupiedCells.reserve(aliveCount_);

//     auto cellKey = [cell_size] (float x, float y) -> std::int64_t {
//             auto cx = static_cast<std::int32_t>(std::floor(x / cell_size));
//             auto cy = static_cast<std::int32_t>(std::floor(y / cell_size));
//             return (static_cast<std::int64_t>(cx) << 32) | (static_cast<std::uint32_t>(cy));
//         };

//     std::size_t count = 0;
//     for (std::size_t i = 0; i < aliveCount_; ++i) {
//         const auto& p = pool_.positions[i];
//         if ((p.x < -1.0f) || (p.x > 1.0f) || (p.y < -1.0f) || (p.y > 1.0f) ) {
//             continue;                                                                 // cull offscreen too
//         }

//         auto key = cellKey(p.x, p.y);
//         if (occupiedCells.insert(key).second) {     // true only if this cell wasn't already taken
//             culled_[count++] = p;
//         }
//     }

//     return {culled_.data(), count};
// }
