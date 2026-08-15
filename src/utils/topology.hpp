#ifndef YARR_UTILS_TOPOLOGY_HPP
#define YARR_UTILS_TOPOLOGY_HPP

#include <cstddef>
#include <string>
#include <vector>


// What the machine says about its own caches and cores. Read once — none of it changes
// while the program runs — and used to size the chunk the simulation works in and to decide
// where its threads should live.
struct Topology
{
    std::size_t l1dBytes {0};
    std::size_t l2Bytes {0};

    // The largest last-level cache on the machine, and the logical CPUs sharing it. Where
    // one die carries stacked cache and another does not — a 7950X3D and its like — this is
    // that die, and a simulation whose working set fits in it wants to run nowhere else. On
    // a uniform machine every CPU reports the same size and this is all of them, which makes
    // pinning a no-op rather than a mistake.
    std::size_t      largestCacheBytes {0};
    std::vector<int> largestCacheCpus;

    int onlineCpus {0};

    // Set when the machine reports one last-level cache shared by every CPU it has. True of
    // most real hardware, and true of a VM whichever way the host is actually built — a
    // hypervisor hands the guest a flattened topology, so an asymmetric host looks uniform
    // from inside it and there is nothing here to pin against.
    bool uniformCache {true};

    // Non-empty when the topology above is known to be the hypervisor's rather than the
    // hardware's, and says which one. Worth showing rather than swallowing: it is the
    // difference between "this machine has nothing to choose between" and "this machine
    // cannot see what there is to choose between".
    std::string virtualized;

    [[nodiscard]] std::size_t particlesPerCache(std::size_t bytesPerParticle) const noexcept
    {
        return (l1dBytes / bytesPerParticle);
    }
};

[[nodiscard]] Topology const& topology();

// === AFFINITY ========================================================================================================
// Confines the process to the CPUs sharing the largest cache. Every thread is moved, not
// just the caller's: the parallel backend's workers are already running by the time anyone
// reaches for this, and only threads created afterwards would inherit it.

[[nodiscard]] bool affinityAvailable() noexcept;

bool pinToLargestCache();
bool restoreAffinity();

#endif // YARR_UTILS_TOPOLOGY_HPP
