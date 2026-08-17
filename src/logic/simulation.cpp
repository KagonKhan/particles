#include "simulation.hpp"

#include <imgui.h>

#include <array>
#include <chrono>
#include <utility>


namespace
{

using Clock = std::chrono::steady_clock;

// Long enough that a single slow step does not swing the number, short enough that the panel
// still reacts to a knob while you are dragging it.
constexpr auto RATE_WINDOW = std::chrono::milliseconds {500};

} // namespace

Simulation::Access::Access(Simulation& owner)
    : owner_{&owner},
      lock_{owner.sceneMutex_, std::defer_lock},
      scene_{owner.scene_.get()}
{
    // Announced before blocking, so the thread sees the intent rather than only the
    // contention it would otherwise be free to ignore.
    owner_->waiters_.fetch_add(1, std::memory_order_release);
    lock_.lock();
}

Simulation::Access::~Access()
{
    lock_.unlock();

    if (owner_->waiters_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // Taken and dropped without doing anything under it: it is what the thread evaluates
        // its predicate under, and acquiring it here is what makes a wakeup impossible to
        // lose between that evaluation and the wait.
        std::scoped_lock const handoff_lock {owner_->handoffMutex_};
        owner_->handoff_.notify_one();
    }
}

Simulation::Simulation()
{
    tickRate_.store(rate_.get(), std::memory_order_relaxed);
    tickScale_.store(timeScale_.get(), std::memory_order_relaxed);
    ticking_.store(running_.get(), std::memory_order_relaxed);

    thread_ = std::jthread {[this] (std::stop_token stop) { run(std::move(stop)); }};
}

void Simulation::run(std::stop_token stop)
{
    auto            next         = Clock::now();
    auto            window_start = next;
    Clock::duration window_cost  = Clock::duration::zero();
    std::size_t     window_steps = 0;

    while (!stop.stop_requested()) {
        if (!ticking_.load(std::memory_order_relaxed)) {
            achievedRate_.store(0.0F, std::memory_order_relaxed);

            std::unique_lock lock {tickMutex_};
            tickWake_.wait(lock, stop, [this] { return ticking_.load(std::memory_order_relaxed); });
            lock.unlock();

            next         = Clock::now();
            window_start = next;
            window_cost  = Clock::duration::zero();
            window_steps = 0;
            continue;
        }

        // The two halves of a step, and the only place they are allowed to differ: how long
        // the step lasts to the simulation, and how long the thread waits before taking the
        // next one. Scaling the first alone is what slow motion is — the same steps, just as
        // often, each worth less time.
        int const   rate   = tickRate_.load(std::memory_order_relaxed);
        float const tick   = 1.0F / static_cast<float>(rate);
        float const step   = tick * tickScale_.load(std::memory_order_relaxed);
        auto const  period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float> {tick});

        auto const step_start = Clock::now();

        {
            std::scoped_lock scene_lock {sceneMutex_};
            scene_->update(step);
        }

        auto const now = Clock::now();

        // The whole of the yield, and it belongs here rather than anywhere else: between two
        // steps is the only moment the scene is not being written to.
        if (waiters_.load(std::memory_order_acquire) > 0) {
            std::unique_lock handoff_lock {handoffMutex_};
            handoff_.wait(handoff_lock, [this] { return waiters_.load(std::memory_order_acquire) == 0; });
        }

        ++window_steps;
        window_cost += now - step_start;

        if (auto const window = now - window_start; (window >= RATE_WINDOW)) {
            auto const steps = static_cast<float>(window_steps);

            achievedRate_.store(steps / std::chrono::duration<float> {window}.count(), std::memory_order_relaxed);
            stepCostMicros_.store(
                std::chrono::duration<float, std::micro> {window_cost}.count() / steps,
                std::memory_order_relaxed);

            window_start = now;
            window_steps = 0;
            window_cost  = Clock::duration::zero();
        }

        next += period;

        // A step that outlasted its own period leaves nothing to make up: the simulation runs
        // slower than wall time instead of chasing the shortfall, which is what stops one slow
        // step from compounding into a spiral of them. Every step is still exactly 1 / rate,
        // so nothing in the simulation jumps — there are only fewer of them per second.
        if (next < now) {
            next = now;
            continue;
        }

        std::unique_lock lock {tickMutex_};
        tickWake_.wait_until(lock, stop, next, [] { return false; });
    }
}

void Simulation::renderSettings()
{
    std::array<KnobBase*, 3> const knobs {&running_, &rate_, &timeScale_};

    if (renderKnobs(knobs)) {
        tickRate_.store(rate_.get(), std::memory_order_relaxed);
        tickScale_.store(timeScale_.get(), std::memory_order_relaxed);
        ticking_.store(running_.get(), std::memory_order_relaxed);
        tickWake_.notify_all();
    }

    auto const  requested = static_cast<float>(rate_.get());
    float const scale     = timeScale_.get();
    float const tick      = 1000.0F / requested;

    ImGui::Text("Step: %.3f ms", tick * scale);

    // Said out loud only when they have come apart, since the pair of them is the whole of
    // what the scale does.
    if (scale != 1.0F) {
        ImGui::TextDisabled("%.0f%% speed, one every %.3f ms", scale * 100.0F, tick);
    }

    if (!running_.get()) {
        ImGui::TextDisabled("Paused");
        return;
    }

    float const achieved  = achievedRate_.load(std::memory_order_relaxed);
    float const step_cost = stepCostMicros_.load(std::memory_order_relaxed) / 1000.0F;

    ImGui::Text("Stepping at: %.1f Hz", achieved);
    ImGui::Text("Step cost: %.3f ms", step_cost);

    // A frame can only borrow the scene between two steps, so what one step costs is also the
    // fastest the interface can be served while the thread has nothing to wait for.
    if (step_cost > 0.0F) {
        ImGui::SetItemTooltip("Caps the frame rate at %.0f FPS while the thread is saturated", 1000.0F / step_cost);
    }

    if ((achieved > 0.0F) && (achieved < (requested * 0.95F))) {
        ImGui::TextColored(ImVec4 {1.0F, 0.6F, 0.2F, 1.0F}, "Below the requested rate, simulated time runs slow");
    }
}
