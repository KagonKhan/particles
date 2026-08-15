#include "scene.hpp"

#include "utils/utils.hpp"
#include <spdlog/spdlog.h>

namespace
{

// The simulation's integrator, and so the scene's — it belongs to no object in it. Explicit
// Euler: a step is worth exactly what the objects above it put into the velocity, which is
// what makes a knob dragged mid-frame read as an immediate change in the motion.
void integrate(ParticleChunk chunk, float dt)
{
    for (std::size_t i {}; i < chunk.size(); ++i) {
        chunk.positions[i] += chunk.velocities[i] * dt;
    }
}

} // namespace

// One pass over the pool per frame instead of one per object. The pool runs to tens of
// megabytes, well past any cache, so sweeping it once per object made every object pay to
// pull the same particles in from memory again — and the cost grew with the scene rather
// than with the physics. Sliced into chunks, a particle is read in once and every object has
// its say while it is still in L1.
//
// Objects are applied in the order they are written here, and that order is the physics:
// forces first, then the step they produce, then the surfaces that correct it. Bouncing
// before integrating would resolve last frame's contact and hand the result straight back.
void Scene::update(float dt)
{
    elapsed_ += dt;

    if (!emitter_->isEnabled()) {
        return;
    }

    auto const start = Time::measure();

    for (std::size_t first = 0; first < pool_->aliveCount; first += kChunkParticles) {
        ParticleChunk const chunk = pool_->chunk(first);

        attractor_->apply(chunk, dt);
        integrate(chunk, dt);
        boundary_->apply(chunk);
    }

    spdlog::debug("Fused pass time: {}ms", Time::duration(start, Time::measure()).count());

    spdlog::debug("Reap time: {}ms", Time::execution(&Emitter::reap, emitter_, *pool_, dt).count());

    // Last, so a particle spawns exactly where the emitter is and is drawn there before
    // anything has had a chance to move it.
    spdlog::debug("Spawn time: {}ms", Time::execution(&Emitter::spawn, emitter_, *pool_, dt).count());
}

void Scene::renderSettings()
{
    emitter_->renderSettings(*pool_);
    attractor_->renderSettings();
    boundary_->renderSettings();
}
