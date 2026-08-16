#include "particle_buffer.hpp"

#include <spdlog/spdlog.h>
#include <cstring>
#include <stdexcept>

namespace
{

constexpr GLbitfield STORAGE_FLAGS = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

void* createMappedStorage(GLuint buffer, GLsizeiptr bytes)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, bytes, nullptr, STORAGE_FLAGS);

    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bytes, STORAGE_FLAGS);
    if (ptr == nullptr) {
        throw std::runtime_error("Failed to map particle buffer");
    }

    return ptr;
}

} // namespace

ParticleBuffer::ParticleBuffer()
{
    glGenBuffers(BUFFER_COUNT, buffers_.data());

    for (std::size_t i = 0; i < BUFFER_COUNT; ++i) {
        mapped_[i] = createMappedStorage(buffers_[i], MAX_PARTICLES * sizeof(ParticleVector));
    }
}

ParticleBuffer::~ParticleBuffer()
{
    for (std::size_t i = 0; i < BUFFER_COUNT; ++i) {
        if (fences_[i] != nullptr) {
            glDeleteSync(fences_[i]);
        }

        if (buffers_[i] != 0) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffers_[i]);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }

    glDeleteBuffers(BUFFER_COUNT, buffers_.data());
}

GLuint ParticleBuffer::upload(std::span<const ParticleVector> positions)
{
    current_ = (current_ + 1) % BUFFER_COUNT;

    if (fences_[current_] != nullptr) {
        GLenum result = glClientWaitSync(fences_[current_], GL_SYNC_FLUSH_COMMANDS_BIT, 1'000'000'000);
        if ((result == GL_TIMEOUT_EXPIRED) || (result == GL_WAIT_FAILED)) {
            // Falling through overwrites storage the GPU may still be reading, which
            // tears the cloud for a frame. Better than deadlocking on a lost context.
            spdlog::warn("particle buffer fence wait failed/timed out");
        }

        glDeleteSync(fences_[current_]);
        fences_[current_] = nullptr;
    }

    std::memcpy(mapped_[current_], positions.data(), positions.size() * sizeof(ParticleVector));
    count_ = static_cast<GLuint>(positions.size());

    return count_;
}

void ParticleBuffer::fence()
{
    fences_[current_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}
