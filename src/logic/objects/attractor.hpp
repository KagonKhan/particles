#ifndef YARR_LOGIC_ATTRACTOR_HPP
#define YARR_LOGIC_ATTRACTOR_HPP


#include "emitter/emitter.hpp"
#include "logic/knob.hpp"
#include "logic/scene_object.hpp"
#include "utils/rng.hpp"
#include <glm/common.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include <cmath>
#include <numbers>
#include <vector>

class SceneObjectBase {}; // TODO: implement

// Where the range starts fading out, as a fraction of the squared range — 0.64 is the
// outer fifth of the radius, since the loop compares squared distances and 0.8^2 = 0.64.
constexpr float kFadeBegins = 0.64F;

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
        "Range", 1.0F, 0.01F, 20.0F, "%.2f u",
        ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic
    };
    Knob<float> softening {
        "Softening", 0.2F, 0.01F, 1.0F, "%.3f u",
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
    Knob<float> range {"Range", 1.0F, -500.0F, 500.0F, "%.3f"};
    Knob<float> strength {"Strength", -5.0F, -5.0F, 5.0F, "%.3f"};
    Knob<int> inversePower {"Inverse Power", 0, -5, 5, "%.d"};

    std::vector<KnobBase*> knobs()
    {
        return {&range, &strength, &inversePower};
    }
};

class Attractor
{
public:
    void setPosition(glm::vec2 position) { attractorSettings_.object.position = position; }

    void simpleUpdate(std::span<ParticleVector> particles, std::span<ParticleVector> velocities, float dt)
    {
        for (std::size_t i {}; i < particles.size(); ++i) {
            auto& particle = particles[i];

            glm::vec2 offset =
                attractorSettings_.object.position - particle;

            float distance2 = glm::dot(offset, offset);

            if (distance2 > settings_.range.get() * settings_.range.get()) {
                continue;
            }

            float distance = glm::sqrt(distance2);

            if (distance > 0.0001f) {
                glm::vec2 direction = offset / distance;

                float strength = settings_.strength.get() / std::powf(distance, (float)settings_.inversePower.get());

                velocities[i] += direction * strength * dt;
            }
        }
    }

    // void advancedUpdate(std::span<ParticleVector> particles, std::span<ParticleVector> velocities, float dt)
    // {
    //     int const       power    = settings_.falloffPower.get();
    //     float const     strength = settings_.strength.get();
    //     float const     rangeSq  = settings_.range.get() * settings_.range.get();
    //     float const     softenSq = settings_.softening.get() * settings_.softening.get();
    //     glm::vec2 const centre   = attractorSettings_.object.position;

    //     float const swirl    = glm::radians(settings_.swirl.get());
    //     float const swirlCos = std::cos(swirl);
    //     float const swirlSin = std::sin(swirl);

    //     float const dragFactor = std::exp(-settings_.damping.get() * dt);

    //     for (std::size_t i {}; i < particles.size(); ++i) {
    //         glm::vec2 const offset     = centre - particles[i];
    //         float const     distanceSq = glm::dot(offset, offset);

    //         // Squared on both sides, so deciding who is in range costs no square root.
    //         if (distanceSq > rangeSq) {
    //             continue;
    //         }

    //         float const     window      = 1.0F - glm::smoothstep(kFadeBegins, 1.0F, distanceSq / rangeSq);
    //         float const     invDistance = 1.0F / std::sqrt(distanceSq + softenSq);
    //         glm::vec2 const pull {
    //             (offset.x * swirlCos) - (offset.y * swirlSin),
    //             (offset.x * swirlSin) + (offset.y * swirlCos)};

    //         velocities[i] += pull * (strength * intPow(invDistance, power + 1) * window) * dt;
    //         velocities[i] *= 1.0F + ((dragFactor - 1.0F) * window);
    //     }
    // }

    void update(std::span<ParticleVector> particles, std::span<ParticleVector> velocities, float dt)
    {
        simpleUpdate(particles, velocities, dt);
    }

    void renderSettings()
    {
        ImGui::Begin("Attractor Settings");

        for (auto& knob : settings_.knobs()) {
            knob->render();
        }

        if (ImGui::Button("Randomise")) {
            std::vector<KnobBase*> knobs = settings_.knobs();
            randomiseKnobs(knobs, rng_);
        }

        ImGui::SeparatorText("Body");

        attractorSettings_.visible.render();
        ImGui::SliderFloat2("Position", glm::value_ptr(attractorSettings_.object.position), -5.0F, 5.0F);
        ImGui::SliderFloat("Radius", &attractorSettings_.object.radius, 0.0F, 2.0F);

        ImGui::End();
    }

    [[nodiscard]] bool               isVisible() const noexcept { return attractorSettings_.visible.get(); }
    [[nodiscard]] SceneObject const& object() const noexcept    { return attractorSettings_.object; }

private:
    struct AttractingSettings : Simple
    {} settings_;

    struct AttractorSettings
    {
        SceneObject object;
        Knob<bool> visible {"Visible", true};
    } attractorSettings_;

    RNG rng_;
};


#endif // YARR_LOGIC_ATTRACTOR_HPP
