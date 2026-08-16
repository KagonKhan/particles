#include "topology.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>

#ifdef __linux__
    #include <sched.h>
    #include <unistd.h>
#endif

namespace
{

std::filesystem::path const CPU_ROOT {"/sys/devices/system/cpu"};

std::string readTrimmed(std::filesystem::path const& path)
{
    std::ifstream file {path};
    std::string   text;

    if (!std::getline(file, text)) {
        return {};
    }

    while (!text.empty() && (std::isspace(static_cast<unsigned char>(text.back())) != 0)) {
        text.pop_back();
    }

    return text;
}

// The kernel writes these with a unit suffix — "32K", "98304K" — rather than in bytes.
std::size_t parseCacheSize(std::string const& text)
{
    if (text.empty()) {
        return 0;
    }

    std::size_t scale = 1;

    switch (text.back()) {
    case 'K': scale = 1024; break;
    case 'M': scale = 1024 * 1024; break;
    case 'G': scale = 1024 * 1024 * 1024; break;
    default: break;
    }

    return static_cast<std::size_t>(std::strtoull(text.c_str(), nullptr, 10)) * scale;
}

// Size of the given cache as cpu `index` sees it. Cache directories are not numbered
// consistently across machines, so they are matched on what they say they are.
std::size_t cacheSize(int cpu, int level, std::string_view type)
{
    std::filesystem::path const root = CPU_ROOT / ("cpu" + std::to_string(cpu)) / "cache";
    std::error_code             error;

    if (!std::filesystem::exists(root, error)) {
        return 0;
    }

    for (auto const& entry : std::filesystem::directory_iterator {root, error}) {
        if (readTrimmed(entry.path() / "level") != std::to_string(level)) {
            continue;
        }

        // A unified cache is whatever you ask of it; a split one has to be the right half.
        std::string const kind = readTrimmed(entry.path() / "type");

        if ((kind != "Unified") && (kind != type)) {
            continue;
        }

        return parseCacheSize(readTrimmed(entry.path() / "size"));
    }

    return 0;
}

std::string detectVirtualization()
{
    std::string const version = readTrimmed("/proc/version");

    if (version.find("microsoft") != std::string::npos || version.find("WSL") != std::string::npos) {
        return "WSL2";
    }

    std::string const hypervisor = readTrimmed("/sys/hypervisor/type");

    return hypervisor.empty()? std::string {} : hypervisor;
}

Topology build()
{
    Topology result;

#ifdef __linux__
    result.onlineCpus = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#endif

    if (result.onlineCpus <= 0) {
        result.onlineCpus = 1;
        return result;
    }

    result.l1dBytes = cacheSize(0, 1, "Data");
    result.l2Bytes  = cacheSize(0, 2, "Unified");

    // Grouped by size rather than by die: a die is not what matters here, the cache it
    // carries is, and the kernel will name the sharing set for us.
    std::map<std::size_t, std::vector<int>> by_last_level;

    for (int cpu = 0; cpu < result.onlineCpus; ++cpu) {
        std::size_t const size = cacheSize(cpu, 3, "Unified");

        if (size > 0) {
            by_last_level[size].push_back(cpu);
        }
    }

    if (!by_last_level.empty()) {
        auto const& largest = *by_last_level.rbegin();

        result.largestCacheBytes = largest.first;
        result.largestCacheCpus  = largest.second;
        result.uniformCache      = (by_last_level.size() == 1);
    }

    result.virtualized = detectVirtualization();

    // Fall back to something usable rather than nothing: a machine that reports no cache
    // sizes at all still has to be given a chunk size, and 32 KiB is the L1 that every
    // x86-64 part of the last fifteen years has had.
    if (result.l1dBytes == 0) {
        result.l1dBytes = 32 * 1024;
    }

    return result;
}

} // namespace

Topology const& topology()
{
    static Topology const instance = build();
    return instance;
}

#ifdef __linux__

namespace
{

// The mask the process started with, so turning the pin off puts it back where it was
// rather than opening it to every CPU on the machine.
cpu_set_t const& originalAffinity()
{
    static cpu_set_t const original = [] {
        cpu_set_t mask;
        CPU_ZERO(&mask);

        if (sched_getaffinity(0, sizeof(mask), &mask) != 0) {
            for (int cpu = 0; cpu < topology().onlineCpus; ++cpu) {
                CPU_SET(cpu, &mask);
            }
        }

        return mask;
    }();

    return original;
}

bool applyToEveryThread(cpu_set_t const& mask)
{
    std::error_code error;
    bool            applied = false;

    // Threads, not the process: sched_setaffinity moves one thread, and the parallel
    // backend's workers were created before anyone thought to ask about affinity.
    for (auto const& entry : std::filesystem::directory_iterator {"/proc/self/task", error}) {
        auto const tid = static_cast<pid_t>(std::strtol(entry.path().filename().c_str(), nullptr, 10));

        if (sched_setaffinity(tid, sizeof(mask), &mask) == 0) {
            applied = true;
        }
    }

    return applied;
}

} // namespace

bool affinityAvailable() noexcept { return true; }

bool pinToLargestCache()
{
    static_cast<void>(originalAffinity());

    Topology const& machine = topology();

    if (machine.largestCacheCpus.empty()) {
        return false;
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);

    for (int const cpu : machine.largestCacheCpus) {
        CPU_SET(cpu, &mask);
    }

    bool const applied = applyToEveryThread(mask);

    spdlog::info(
        "Affinity: {} to {} of {} CPUs sharing {} MiB",
        applied? "pinned" : "failed to pin",
        machine.largestCacheCpus.size(),
        machine.onlineCpus,
        machine.largestCacheBytes / (1024 * 1024));

    return applied;
}

bool restoreAffinity()
{
    return applyToEveryThread(originalAffinity());
}

#else

bool affinityAvailable() noexcept { return false; }
bool pinToLargestCache() { return false; }
bool restoreAffinity() { return false; }

#endif // __linux__
