#ifndef YARR_PARTICLE_BUFFER_HPP
#define YARR_PARTICLE_BUFFER_HPP

#include "emitter/emitter.hpp"

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
class ParticleBuffer
{
public:
    ParticleBuffer();
    ~ParticleBuffer();

    // Holds raw GL names, so copying one would double-free them.
    ParticleBuffer(ParticleBuffer const&)            = delete;
    ParticleBuffer& operator=(ParticleBuffer const&) = delete;
    ParticleBuffer(ParticleBuffer&&)                 = delete;
    ParticleBuffer& operator=(ParticleBuffer&&)      = delete;

    // Advances to the next buffer in the ring, waits out the GPU if it is somehow still
    // reading that one, and streams the positions in. Returns the particle count.
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
    static constexpr std::size_t kBufferCount {3};

    std::array<GLuint, kBufferCount> buffers_ {};
    std::array<void*, kBufferCount>  mapped_ {};
    std::array<GLsync, kBufferCount> fences_ {};

    std::size_t current_ {0};
    GLuint      count_ {0};
};

#endif // YARR_PARTICLE_BUFFER_HPP
