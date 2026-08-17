#ifndef YARR_LOGIC_SIMULATION_HPP
#define YARR_LOGIC_SIMULATION_HPP

#include "logic/scene.hpp"
#include "utils/bases.hpp"
#include "utils/knob.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>


// The simulation owns its clock and its thread. A step lasts 1 / rate and happens every
// 1 / rate of wall time, and the frame loop has no say in either — a frame that takes 200 ms
// costs the simulation only the moment it spends handing the pool over. Nothing accumulates
// between frames, so there is no per-frame step budget to run out of and no backlog to replay.
class Simulation : public Immovable<Simulation>
{
public:
    // The scene, for as long as the guard lives. The thread cannot step while it is held, so
    // this is for the short windows a frame needs — reading the pool, applying input, drawing
    // the scene's own panels — and never for a whole frame.
    class Access
    {
    public:
        ~Access();

        // Never moved or copied: `borrow`'s prvalue is constructed in place, and a moved-from
        // guard would have to decide which of the two ends the borrow.
        Access(Access const&)             = delete;
        Access(Access&&)                  = delete;
        Access& operator =(Access const&) = delete;
        Access& operator =(Access&&)      = delete;

        Scene* operator ->() const noexcept { return scene_; }
        Scene& operator *() const noexcept  { return *scene_; }

    private:
        friend class Simulation;

        explicit Access(Simulation& owner);

        Simulation*                  owner_;
        std::unique_lock<std::mutex> lock_;
        Scene*                       scene_;
    };

    Simulation();

    [[nodiscard]] Access borrow() { return Access {*this}; }

    ///@brief The rate and pause knobs, and what the thread actually managed. Main thread.
    void renderSettings();

private:
    void run(std::stop_token stop);

    std::unique_ptr<Scene> scene_ {std::make_unique<Scene>()};
    std::mutex             sceneMutex_;

    Knob<bool> running_ {
        "Running", true,
        "Whether the thread is stepping. Paused, it waits rather than spinning, and the scene\nstays available to look at and edit."
    };
    Knob<int> rate_ {
        "Simulation Rate", 120, 1, 480, "%d Hz",
        "How often a step happens: one per 1 / rate of wall time. Independent of the frame rate."
    };
    Knob<float> timeScale_ {
        "Time Scale", 1.0F, 0.01F, 4.0F, "%.2fx",
        "Simulated time per step, as a multiple of the rate's. Slow motion costs nothing —\nthe same steps happen just as often, each covering less time, so the motion stays\nsmooth and the integration gets finer rather than coarser.",
        ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic
    };

    // What the thread reads. The knobs themselves belong to the frame loop that draws them,
    // so their values are pushed across here rather than read from another thread.
    std::atomic<int>   tickRate_ {120};
    std::atomic<float> tickScale_ {1.0F};
    std::atomic<bool>  ticking_ {true};
    std::atomic<float> achievedRate_ {0.0F};
    std::atomic<float> stepCostMicros_ {0.0F};

    std::mutex                  tickMutex_;
    std::condition_variable_any tickWake_;

    // Borrows outstanding, and the thread's promise not to start another step while any of
    // them are waiting. A saturated thread holds the scene mutex for the whole of every step
    // and frees it for the hundred nanoseconds between two, and glibc's mutex is not fair —
    // a waiter that has to be scheduled first loses that race essentially every time. The
    // handoff replaces the race with a rule.
    std::atomic<int>        waiters_ {0};
    std::mutex              handoffMutex_;
    std::condition_variable handoff_;

    // Last, so it starts once everything it touches exists and is joined before any of it
    // goes away.
    std::jthread thread_;
};

#endif // YARR_LOGIC_SIMULATION_HPP
