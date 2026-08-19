#include "scene.hpp"

#include "utils/topology.hpp"
#include "utils/utils.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>

#ifdef PARTICLES_HAVE_TBB
    #include <execution>

    #if defined(__GLIBCXX__) && !defined(_PSTL_PAR_BACKEND_TBB)
        #error "TBB is linked but <execution> selected its serial backend — TBB's headers are not on the include path"
    #endif
#endif

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

void Scene::update(float dt)
{
    elapsed_ += dt;

    if (!emitter_->isEnabled()) {
        return;
    }

    auto const start = Time::measure();

    chunks_.clear();

    for (std::size_t first = 0; first < pool_->aliveCount; first += chunkParticles_) {
        chunks_.push_back(pool_->chunk(first, chunkParticles_));
    }

    auto const step = [this, dt] (ParticleChunk chunk) {
            attractor_->apply(chunk, dt);
            integrate(chunk, dt);
        };

#ifdef PARTICLES_HAVE_TBB
    if (parallel_) {
        std::for_each(std::execution::par, chunks_.begin(), chunks_.end(), step);
    }
    else
#endif
    {
        std::for_each(chunks_.begin(), chunks_.end(), step);
    }

    lastPassMicros_ =
        static_cast<double>(Time::duration<std::chrono::nanoseconds>(start, Time::measure()).count()) / 1000.0;

    bench_.sample(lastPassMicros_, pool_->aliveCount);

    emitter_->reap(*pool_, dt);

    // Last, so a particle spawns exactly where the emitter is and is drawn there before
    // anything has had a chance to move it.
    emitter_->spawn(*pool_, dt);
}

void Scene::renderSettings()
{
    renderTuning();
}

void Scene::renderTuning()
{
    Topology const& machine = topology();

    ImGui::Begin("Simulation Tuning");

    ImGui::Text("Fused pass: %.1f us", lastPassMicros_);
    ImGui::Text("Chunks: %zu", chunks_.size());

    ImGui::SeparatorText("Chunk size");

    int chunk = static_cast<int>(chunkParticles_);

    if (ImGui::SliderInt("Particles", &chunk, 64, 65536, "%d", ImGuiSliderFlags_Logarithmic)) {
        chunkParticles_ = static_cast<std::size_t>(std::max(chunk, 1));
    }

    std::size_t const chunk_bytes = chunkParticles_ * CHUNK_BYTES_PER_PARTICLE;

    ImGui::Text("%zu KiB per chunk", chunk_bytes / 1024);
    ImGui::SameLine();

    // Which cache a chunk actually lands in is the only thing the number means. Said out
    // loud, since it is not recoverable from the slider.
    if (chunk_bytes <= machine.l1dBytes) {
        ImGui::TextDisabled("(fits L1d, %zu KiB)", machine.l1dBytes / 1024);
    }
    else if (chunk_bytes <= machine.l2Bytes) {
        ImGui::TextDisabled("(spills to L2, %zu KiB)", machine.l2Bytes / 1024);
    }
    else {
        ImGui::TextDisabled("(past L2 — spilling to the shared cache)");
    }

    if (ImGui::SmallButton("Fit L1d")) {
        chunkParticles_ = std::max<std::size_t>(64, machine.particlesPerCache(CHUNK_BYTES_PER_PARTICLE) / 2);
    }

    ImGui::SetItemTooltip("Half of L1d, leaving room for what the objects themselves touch");

    ImGui::SeparatorText("Threading");

#ifdef PARTICLES_HAVE_TBB
    ImGui::Checkbox("Parallel", &parallel_);
    ImGui::SetItemTooltip(
        "Chunks across threads, via the parallel STL. Chunks never share particles, so there is nothing to synchronise.");
#else
    bool unavailable = false;
    ImGui::BeginDisabled();
    ImGui::Checkbox("Parallel", &unavailable);
    ImGui::EndDisabled();
    ImGui::TextWrapped(
        "Built without TBB. libstdc++ implements the parallel execution policies on top of it, "
        "and without it std::execution::par runs serially with no diagnostic — so the toggle is "
        "disabled rather than lying. Re-run conan install and rebuild.");
#endif

    ImGui::Text("Hardware threads: %d", machine.onlineCpus);

    ImGui::SeparatorText("Affinity");

    if (!affinityAvailable()) {
        ImGui::TextDisabled("No affinity control on this platform");
    }
    else if (machine.uniformCache) {
        // Not a failure, and worth distinguishing from one. A machine whose CPUs all share
        // one last-level cache has nothing to pin against; a guest is merely unable to see
        // what its host has.
        ImGui::BeginDisabled();
        bool nothing_to_pin = false;
        ImGui::Checkbox("Pin to largest cache", &nothing_to_pin);
        ImGui::EndDisabled();

        if (machine.virtualized.empty()) {
            ImGui::TextWrapped(
                "All %d CPUs report one %zu MiB last-level cache, so there is nothing to choose between.",
                machine.onlineCpus,
                machine.largestCacheBytes / (1024 * 1024));
        }
        else {
            ImGui::TextWrapped(
                "Running under %s, which reports a flattened topology: all %d CPUs claim one %zu MiB "
                "cache and one die. A host with stacked cache on one die only cannot be seen from in "
                "here, and a guest CPU is not a fixed host core in any case — pin from the host side.",
                machine.virtualized.c_str(),
                machine.onlineCpus,
                machine.largestCacheBytes / (1024 * 1024));
        }
    }
    else if (ImGui::Checkbox("Pin to largest cache", &pinned_)) {
        bool const applied = pinned_? pinToLargestCache() : restoreAffinity();

        if (!applied) {
            pinned_ = false;
        }
    }

    if (!machine.uniformCache) {
        ImGui::SetItemTooltip(
            "Confines every thread to the %zu CPUs sharing %zu MiB",
            machine.largestCacheCpus.size(),
            machine.largestCacheBytes / (1024 * 1024));
    }

    bench_.render(
        RunConfig {
            .chunkParticles = chunkParticles_,
            .parallel       = parallel_,
            .pinned         = pinned_,
            .threads        = pinned_? static_cast<int>(machine.largestCacheCpus.size()) : machine.onlineCpus,
        });

    ImGui::End();
}
