#ifndef YARR_SCENE_HPP
#define YARR_SCENE_HPP

#include "logic/objects/attractor.hpp"
#include "logic/objects/boundary.hpp"
#include "logic/objects/emitter.hpp"
#include "logic/particle_pool.hpp"
#include "utils/bench.hpp"

#include <glm/ext/vector_float2.hpp>
#include <memory>
#include <span>
#include <vector>

class Scene
{
public:
    // One simulation step. The caller decides how often a step happens and how long it
    // lasts — dt here is the simulation's, not the frame's.
    void update(float dt);
    void renderSettings();

    void placeEmitter(glm::vec2 position) { attractor_->setPosition(position); }

    [[nodiscard]] std::span<const ParticleVector> positions() const noexcept { return pool_->alivePositions(); }
    [[nodiscard]] float const*                    ages() const noexcept      { return pool_->ages.data(); }
    [[nodiscard]] float                           elapsed() const noexcept   { return static_cast<float>(elapsed_); }

    [[nodiscard]] std::vector<SceneObject const*> getSceneObjects() const noexcept
    {
        return {&emitter_->object(), &attractor_->object()};
    }

    // Whether a run is in progress and the frame should be spent on the simulation rather
    // than on drawing it. The caller decides what to do about it — the scene has no opinion
    // on rendering — but only the bench knows a run is under way.
    [[nodiscard]] bool benchmarking() const noexcept { return bench_.recording(); }

private:
    // How the fused pass is run, rather than what it computes. Separated out because it is
    // the part with no right answer known ahead of time: it is measured, not reasoned about.
    void renderTuning();

    // On the heap: the pool runs to tens of megabytes, and Scene is a value member of a
    // stack-allocated App.
    std::unique_ptr<ParticlePool> pool_ {std::make_unique<ParticlePool>()};

    std::unique_ptr<Emitter>   emitter_ {std::make_unique<Emitter>()};
    std::unique_ptr<Attractor> attractor_ {std::make_unique<Attractor>()};
    // std::unique_ptr<Boundary>  boundary_ {std::make_unique<Boundary>()};
    double elapsed_ {0.0};

    // Rebuilt each step and kept between them for its capacity. The parallel backend wants a
    // real range of real objects to divide up, and this is it — a few hundred chunks against
    // a million particles, so building it costs nothing next to what it describes.
    std::vector<ParticleChunk> chunks_;

    std::size_t chunkParticles_ {kChunkParticles};
    bool        parallel_ {true};
    bool        pinned_ {false};

    Bench  bench_;
    double lastPassMicros_ {0.0};
};

#endif // YARR_SCENE_HPP
