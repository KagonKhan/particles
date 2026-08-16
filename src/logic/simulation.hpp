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
        Scene* operator ->() const noexcept { return scene_; }
        Scene& operator *() const noexcept  { return *scene_; }

    private:
        friend class Simulation;

        Access(std::mutex& mutex, Scene& scene)
            : lock_{mutex},
              scene_{&scene}
        {}

        std::unique_lock<std::mutex> lock_;
        Scene*                       scene_;
    };

    Simulation();

    [[nodiscard]] Access borrow() { return Access {sceneMutex_, *scene_}; }

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
        "Step size and step frequency both: one step per 1 / rate of wall time, each worth\n1 / rate of simulated time. Independent of the frame rate."
    };

    // What the thread reads. The knobs themselves belong to the frame loop that draws them,
    // so their values are pushed across here rather than read from another thread.
    std::atomic<int>   tickRate_ {120};
    std::atomic<bool>  ticking_ {true};
    std::atomic<float> achievedRate_ {0.0F};

    std::mutex                  tickMutex_;
    std::condition_variable_any tickWake_;

    // Last, so it starts once everything it touches exists and is joined before any of it
    // goes away.
    std::jthread thread_;
};

#endif // YARR_LOGIC_SIMULATION_HPP
