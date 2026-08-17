#ifndef YARR_PARTICLE_BUFFER_HPP
#define YARR_PARTICLE_BUFFER_HPP

#include "logic/particle_pool.hpp"
#include "utils/logger.hpp"

#include <GL/glew.h>

#include <array>
#include <cstddef>
#include <span>

// The particle pool, streamed to the GPU once per frame and shared by every pipeline
// that wants to draw it. One allocation, because at MAX_PARTICLES the storage runs to
// gigabytes and no pipeline can afford a private copy.
//
// Persistently mapped and triple buffered: the CPU writes buffer N while the GPU is
// still reading N-1, and a fence per buffer keeps the two from meeting.
class ParticleBuffer : private Logger<ParticleBuffer>
{
public:
    ParticleBuffer();
    ~ParticleBuffer();

    // Holds raw GL names, so copying one would double-free them.
    ParticleBuffer(ParticleBuffer const&)            = delete;
    ParticleBuffer& operator=(ParticleBuffer const&) = delete;
    ParticleBuffer(ParticleBuffer&&)                 = delete;
    ParticleBuffer& operator=(ParticleBuffer&&)      = delete;

    // Advances to the next buffer in the ring and waits out the GPU if it is somehow still
    // reading that one. Separate from the upload because it is the one part of streaming a
    // frame that can block on the GPU, and the pool it is about to copy from belongs to a
    // thread that must not be made to wait on that.
    void acquire();

    // Streams the positions into the buffer `acquire` picked. Returns the particle count.
    GLuint upload(std::span<const ParticleVector> positions);

    // Marks the command stream past every draw that read this frame's buffer. Call once
    // after the last pipeline has issued its draw.
    void fence();

    [[nodiscard]] GLuint buffer() const noexcept { return buffers_[current_]; }
    [[nodiscard]] GLuint count() const noexcept { return count_; }

    [[nodiscard]] static constexpr GLsizei stride() noexcept
    {
        return static_cast<GLsizei>(sizeof(ParticleVector));
    }

private:
    // Frames of latency between writing a mapped buffer and the GPU being done reading
    // it. Three is enough that the fence is essentially never hit.
    static constexpr std::size_t BUFFER_COUNT {3};

    std::array<GLuint, BUFFER_COUNT> buffers_ {};
    std::array<void*, BUFFER_COUNT>  mapped_ {};
    std::array<GLsync, BUFFER_COUNT> fences_ {};

    std::size_t current_ {0};
    GLuint      count_ {0};
};

#endif // YARR_PARTICLE_BUFFER_HPP
