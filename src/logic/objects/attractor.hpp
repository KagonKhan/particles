#ifndef YARR_LOGIC_ATTRACTOR_HPP
#define YARR_LOGIC_ATTRACTOR_HPP


#include "logic/particle_pool.hpp"
#include "logic/scene_object.hpp"
#include "utils/knob.hpp"
#include "utils/math_utils.hpp"
#include "utils/rng.hpp"

#include <glm/common.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include <cmath>
#include <limits>
#include <span>
#include <vector>

// Where the range starts fading out, as a fraction of the squared range — 0.64 is the
// outer fifth of the radius, since the loop compares squared distances and 0.8^2 = 0.64.
constexpr float FADE_BEGINS = 0.64F;

// How far a single step may carry a particle towards the attractor, as a fraction of its
// distance to the centre. At a strong power the force reaches 1e5 u/s^2 and beyond — growing
// with distance one way, singular at the centre the other — and explicit Euler answers that
// with one step long enough to leave the range for good. Half the distance is the most a step
// can move a particle and still be describing the force it was sampled at.
constexpr float MAX_STEP_FRACTION = 0.5F;


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
    Knob<bool> stepClamp {
        "Step Clamp", true,
        "Caps the kick at what moves a particle half its distance to the centre in one step. "
        "Off, a strong power throws it far enough that it never comes back — alive, but never "
        "drawn again."
    };

    std::vector<KnobBase*> knobs()
    {
        return {&range, &strength, &inversePower, &stepClamp};
    }
};

class Attractor
{
public:
    void setPosition(glm::vec2 position) { object_.transform.position = position; }

    void simpleApply(ParticleChunk chunk, float dt) const
    {
        glm::vec2 const centre   = object_.transform.position;
        float const     range_sq = settings_.range.get() * settings_.range.get();
        float const     scale    = settings_.strength.get();
        float const     power    = -settings_.inversePower.get();

        // Off, the cap is infinite and the clamp below is a no-op — the toggle costs one
        // branch per chunk rather than one per particle.
        float const step_cap = settings_.stepClamp.get()
                ? MAX_STEP_FRACTION / (dt * dt)
                : std::numeric_limits<float>::infinity();

        for (std::size_t i {}; i < chunk.size(); ++i) {
            glm::vec2 const offset    = centre - chunk.positions[i];
            float const     distance2 = glm::dot(offset, offset);

            if (distance2 > range_sq) {
                continue;
            }

            float const distance = glm::sqrt(distance2);

            if (distance > 0.0001F) {
                glm::vec2 const direction = offset / distance;
                float const     unbounded = scale / std::powf(distance, power);
                float const     cap       = step_cap * distance;
                float const     strength  = glm::clamp(unbounded, -cap, cap);

                chunk.velocities[i] += direction * strength * dt;
            }
        }
    }

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
