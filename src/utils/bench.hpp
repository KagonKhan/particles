#ifndef YARR_UTILS_BENCH_HPP
#define YARR_UTILS_BENCH_HPP

#include "utils/logger.hpp"

#include <cstddef>
#include <string>
#include <vector>


struct RunConfig
{
    std::size_t chunkParticles {0};
    bool parallel {false};
    bool pinned {false};
    int threads {0};
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

// TODO: refactor, but low priority right now
class Bench : private Logger<Bench>
{
public:
    void start(std::string label, RunConfig config);
    void cancel();
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

    State       state_ {State::IDLE};
    RunConfig   config_;
    std::string label_ {"run"};

    int warmupSteps_ {120};
    int sampleSteps_ {600};

    int                      warmupSeen_ {0};
    std::vector<double>      micros_;
    std::vector<std::size_t> alive_;

    std::vector<RunResult> history_;
};

#endif // YARR_UTILS_BENCH_HPP
