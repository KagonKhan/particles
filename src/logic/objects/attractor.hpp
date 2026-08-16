#ifndef YARR_LOGIC_ATTRACTOR_HPP
#define YARR_LOGIC_ATTRACTOR_HPP


#include "logic/particle_pool.hpp"
#include "logic/scene_object.hpp"
#include "utils/knob.hpp"
#include "utils/rng.hpp"
#include <glm/common.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include <cmath>
#include <numbers>
#include <span>
#include <vector>

// Where the range starts fading out, as a fraction of the squared range — 0.64 is the
// outer fifth of the radius, since the loop compares squared distances and 0.8^2 = 0.64.
constexpr float FADE_BEGINS = 0.64F;

// Raising a float to a small integer power. std::pow takes its slow general path for a
// float exponent, and this runs once per particle per frame — at a million particles that
// difference is most of the frame budget.
[[nodiscard]] constexpr float intPow(float base, int exponent) noexcept
{
    float     result    = 1.0F;
    int const magnitude = exponent < 0? -exponent : exponent;

    for (int i = 0; i < magnitude; ++i) {
        result *= base;
    }

    return (exponent < 0)? 1.0F / result : result;
}

struct Advanced
{
    Knob<float> strength {"Strength", 1.0F, -5.0F, 5.0F, "%.3f u/s^2 @ 1u"};
    Knob<float> range {
        "Range", 1.0F, 0.01F, 20.0F, "%.2f u", "",
        ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic
    };
    Knob<float> softening {
        "Softening", 0.2F, 0.01F, 1.0F, "%.3f u", "",
        ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic
    };

    Knob<int> falloffPower {"Falloff power", 2, -1, 4, "%d"};
    Knob<float> swirl {"Swirl", 0.0F, -180.0F, 180.0F, "%.0f deg"};
    Knob<float> damping {"Damping", 0.0F, 0.0F, 3.0F, "%.2f /s"};

    std::vector<KnobBase*> knobs()
    {
        return {&strength, &range, &softening, &falloffPower, &swirl, &damping};
    }
};


struct Simple
{
    Knob<float> range {"Range", 1.0F, 0.01F, 500.0F, "%.3f"};
    Knob<float> strength {"Strength", -5.0F, -100.0F, 100.0F, "%.3f"};
    Knob<float> inversePower {"Inverse Power", 1.0F, -5.0F, 5.0F, "%.3f"};

    std::vector<KnobBase*> knobs()
    {
        return {&range, &strength, &inversePower};
    }
};

class Attractor
{
public:
    void setPosition(glm::vec2 position) { object_.transform.position = position; }

    // A chunk at a time, so the scene can put every object's kernel over one slice of the
    // pool while it is in cache rather than sweeping the whole pool once per object.
    void simpleApply(ParticleChunk chunk, float dt) const
    {
        // Read once for the chunk. Out of the loop these are constants the compiler can keep
        // in registers; inside it, each is a load through `this` that it has to assume any
        // write to a particle might have invalidated.
        glm::vec2 const centre   = object_.transform.position;
        float const     range_sq = settings_.range.get() * settings_.range.get();
        float const     scale    = settings_.strength.get();
        float const     power    = -settings_.inversePower.get();

        for (std::size_t i {}; i < chunk.size(); ++i) {
            glm::vec2 const offset    = centre - chunk.positions[i];
            float const     distance2 = glm::dot(offset, offset);

            if (distance2 > range_sq) {
                continue;
            }

            float const distance = glm::sqrt(distance2);

            if (distance > 0.0001F) {
                glm::vec2 const direction = offset / distance;
                float const     strength  = scale / std::powf(distance, power);

                chunk.velocities[i] += direction * strength * dt;
            }
        }
    }

    // void advancedUpdate(std::span<ParticleVector> particles, std::span<ParticleVector> velocities, float dt)
    // {
    //     int const       power     = settings_.falloffPower.get();
    //     float const     strength  = settings_.strength.get();
    //     float const     range_sq  = settings_.range.get() * settings_.range.get();
    //     float const     soften_sq = settings_.softening.get() * settings_.softening.get();
    //     glm::vec2 const centre    = object_.transform.position;

    //     float const swirl     = glm::radians(settings_.swirl.get());
    //     float const swirl_cos = std::cos(swirl);
    //     float const swirl_sin = std::sin(swirl);

    //     float const drag_factor = std::exp(-settings_.damping.get() * dt);

    //     for (std::size_t i {}; i < particles.size(); ++i) {
    //         glm::vec2 const offset      = centre - particles[i];
    //         float const     distance_sq = glm::dot(offset, offset);

    //         // Squared on both sides, so deciding who is in range costs no square root.
    //         if (distance_sq > range_sq) {
    //             continue;
    //         }

    //         float const     window       = 1.0F - glm::smoothstep(FADE_BEGINS, 1.0F, distance_sq / range_sq);
    //         float const     inv_distance = 1.0F / std::sqrt(distance_sq + soften_sq);
    //         glm::vec2 const pull {
    //             (offset.x * swirl_cos) - (offset.y * swirl_sin),
    //             (offset.x * swirl_sin) + (offset.y * swirl_cos)};

    //         velocities[i] += pull * (strength * intPow(inv_distance, power + 1) * window) * dt;
    //         velocities[i] *= 1.0F + ((drag_factor - 1.0F) * window);
    //     }
    // }

    void apply(ParticleChunk chunk, float dt) const
    {
        simpleApply(chunk, dt);
    }

    void renderSettings()
    {
        ImGui::Begin("Attractor Settings");

        for (auto& knob : settings_.knobs()) {
            knob->render();
        }

        ImGui::SeparatorText("Body");

        renderSceneObjectSettings(object_);

        ImGui::End();
    }

    [[nodiscard]] bool               isVisible() const noexcept { return object_.visible; }
    [[nodiscard]] SceneObject const& object() const noexcept    { return object_; }

private:
    struct AttractingSettings : Simple
    {} settings_;

    SceneObject object_ {
        . transform = {},
        .shape      = Circle {},
        .height     = 0.25F,
        .color      = {0.55F, 0.40F, 0.80F, 1.0F},
        .visible    = true,
    };

    Rng rng_;
};


#endif // YARR_LOGIC_ATTRACTOR_HPP
