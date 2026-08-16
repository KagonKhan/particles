#ifndef YARR_UTILS_BENCH_HPP
#define YARR_UTILS_BENCH_HPP

#include <cstddef>
#include <string>
#include <vector>


// What a run was measuring. Written into the summary beside the numbers, because a timing
// without the configuration that produced it cannot be compared with anything.
struct RunConfig
{
    std::size_t chunkParticles {0};
    bool        parallel {false};
    bool        pinned {false};
    int         threads {0};
};

struct RunResult
{
    std::string label;
    std::size_t samples {0};

    double meanMicros {0.0};
    double medianMicros {0.0};
    double p95Micros {0.0};
    double minMicros {0.0};
    double maxMicros {0.0};
    double stddevMicros {0.0};

    std::size_t minAlive {0};
    std::size_t maxAlive {0};

    std::string path;
};


// Records how long the simulation's fused pass takes, a step at a time, and writes the run
// out so two configurations can be compared rather than remembered.
//
// Nothing is written while a run is in progress: samples go to memory, and the file is
// produced once the run ends. A run that wrote as it went would be measuring the filesystem
// as much as the simulation.
class Bench
{
public:
    void start(std::string label, RunConfig config);
    void cancel();

    // One simulation step. Warm-up steps are counted and discarded; the run ends itself once
    // it has the samples it was asked for.
    void sample(double micros, std::size_t alive);

    [[nodiscard]] bool recording() const noexcept { return state_ != State::IDLE; }

    void render(RunConfig const& current);

private:
    enum class State
    {
        IDLE,
        WARMUP,
        COLLECTING,
    };

    void finish();

    State     state_ {State::IDLE};
    RunConfig config_;
    std::string label_ {"run"};

    // Discarded before anything is kept. A simulation that has just been re-configured is
    // not yet the thing being measured: the pool is still filling, the caches hold the
    // previous layout, and the first steps after a change are all transient.
    int warmupSteps_ {120};
    int sampleSteps_ {600};

    int                 warmupSeen_ {0};
    std::vector<double> micros_;
    std::vector<std::size_t> alive_;

    std::vector<RunResult> history_;
};

#endif // YARR_UTILS_BENCH_HPP
