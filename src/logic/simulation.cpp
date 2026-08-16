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

Simulation::Simulation()
{
    tickRate_.store(rate_.get(), std::memory_order_relaxed);
    ticking_.store(running_.get(), std::memory_order_relaxed);

    thread_ = std::jthread {[this] (std::stop_token stop) { run(std::move(stop)); }};
}

void Simulation::run(std::stop_token stop)
{
    auto        next         = Clock::now();
    auto        window_start = next;
    std::size_t window_steps = 0;

    while (!stop.stop_requested()) {
        if (!ticking_.load(std::memory_order_relaxed)) {
            achievedRate_.store(0.0F, std::memory_order_relaxed);

            std::unique_lock lock {tickMutex_};
            tickWake_.wait(lock, stop, [this] { return ticking_.load(std::memory_order_relaxed); });
            lock.unlock();

            next         = Clock::now();
            window_start = next;
            window_steps = 0;
            continue;
        }

        int const   rate   = tickRate_.load(std::memory_order_relaxed);
        float const step   = 1.0F / static_cast<float>(rate);
        auto const  period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float> {step});

        {
            std::scoped_lock scene_lock {sceneMutex_};
            scene_->update(step);
        }

        ++window_steps;

        auto const now = Clock::now();

        if (auto const window = now - window_start; (window >= RATE_WINDOW)) {
            achievedRate_.store(
                static_cast<float>(window_steps) / std::chrono::duration<float> {window}.count(),
                std::memory_order_relaxed);

            window_start = now;
            window_steps = 0;
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
    std::array<KnobBase*, 2> const knobs {&running_, &rate_};

    if (renderKnobs(knobs)) {
        tickRate_.store(rate_.get(), std::memory_order_relaxed);
        ticking_.store(running_.get(), std::memory_order_relaxed);
        tickWake_.notify_all();
    }

    auto const requested = static_cast<float>(rate_.get());

    ImGui::Text("Step: %.3f ms", 1000.0F / requested);

    if (!running_.get()) {
        ImGui::TextDisabled("Paused");
        return;
    }

    float const achieved = achievedRate_.load(std::memory_order_relaxed);

    ImGui::Text("Stepping at: %.1f Hz", static_cast<double>(achieved));

    if ((achieved > 0.0F) && (achieved < (requested * 0.95F))) {
        ImGui::TextColored(ImVec4 {1.0F, 0.6F, 0.2F, 1.0F}, "Below the requested rate, simulated time runs slow");
    }
}
