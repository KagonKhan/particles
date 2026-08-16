#ifndef YARR_SCENE_HPP
#define YARR_SCENE_HPP

#include "logic/objects/attractor.hpp"
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

    // By value: the caller is a frame that has borrowed the simulation for a moment, and
    // what it draws must not be a pointer back into a scene that steps again the instant it
    // lets go.
    [[nodiscard]] std::vector<SceneObject> getSceneObjects() const
    {
        return {emitter_->object(), attractor_->object()};
    }

    [[nodiscard]] bool benchmarking() const noexcept { return bench_.recording(); }

private:
    void renderTuning();

    std::unique_ptr<ParticlePool> pool_ {std::make_unique<ParticlePool>()};

    std::unique_ptr<Emitter>   emitter_ {std::make_unique<Emitter>()};
    std::unique_ptr<Attractor> attractor_ {std::make_unique<Attractor>()};
    double                     elapsed_ {0.0};

    // Rebuilt each step and kept between them for its capacity. The parallel backend wants a
    // real range of real objects to divide up, and this is it — a few hundred chunks against
    // a million particles, so building it costs nothing next to what it describes.
    std::vector<ParticleChunk> chunks_;

    std::size_t chunkParticles_ {CHUNK_PARTICLES};
    bool        parallel_ {true};
    bool        pinned_ {false};

    Bench  bench_;
    double lastPassMicros_ {0.0};
};

#endif // YARR_SCENE_HPP
