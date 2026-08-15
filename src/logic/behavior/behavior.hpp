#ifndef YARR_LOGIC_BEHAVIOR_HPP
#define YARR_LOGIC_BEHAVIOR_HPP

#include "logic/knob.hpp"
#include "logic/particle_pool.hpp"
#include "logic/scene_object.hpp"

#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>
#include <imgui.h>

#include <array>
#include <cstddef>

// How far off the surface a resolved particle is left. Landing it exactly on the outline
// lets rounding put it back inside, and it would spend every frame being pushed out again.
constexpr float kContactSkin = 1e-4F;


struct Behavior
{
public:
    Behavior()          = default;
    virtual ~Behavior() = default;

    virtual void update(ParticlePool& pool, SceneObject const& object) const = 0;
    virtual void renderKnobs()                                               = 0;

protected:
    Behavior(Behavior const&)             = default;
    Behavior(Behavior&&)                  = default;
    Behavior& operator =(Behavior const&) = default;
    Behavior& operator =(Behavior&&)      = default;
};

struct Bounce : public Behavior
{
    // The object arrives per call rather than in the constructor, so one behavior can serve
    // every body in the scene and a shape dragged on its sliders takes effect that frame.
    void update(ParticlePool& pool, SceneObject const& object) const override
    {
        float const restitution = bounceFactor_.get();
        float const slide       = 1.0F - friction_.get();
        float const rest        = restThreshold_.get();

        for (std::size_t i {}; i < pool.aliveCount; ++i) {
            glm::vec2&    position = pool.positions[i];
            glm::vec2&    velocity = pool.velocities[i];
            Contact const hit      = contact(object.shape, position - object.transform.position);

            if (hit.distance >= kContactSkin) {
                continue;
            }

            position += hit.normal * (kContactSkin - hit.distance);

            float const approach = glm::dot(velocity, hit.normal);

            // Already on its way out — reflecting it again is what makes particles buzz
            // against a surface instead of leaving it.
            if (approach >= 0.0F) {
                continue;
            }

            // Split at the normal so the two halves of a contact stay independent:
            // restitution decides how much of the impact comes back, friction how much of
            // the slide across the surface survives it.
            glm::vec2 const tangential = velocity - (approach * hit.normal);
            float const     rebound    = (-approach > rest)? -approach * restitution : 0.0F;

            velocity = (tangential * slide) + (hit.normal * rebound);
        }
    }

    void renderKnobs() override
    {
        ImGui::SeparatorText("Bouncing");
        bounceFactor_.render();
        friction_.render();
        restThreshold_.render();
    }

private:
    Knob<float> bounceFactor_ {"Bounce Factor", 1.0F, 0.0F, 5.0F, "%.3f"};
    Knob<float> friction_ {"Friction", 0.0F, 0.0F, 1.0F, "%.3f"};

    // Under this much approach speed a particle is taken as resting and only slides. Left
    // to bounce it trades a fraction of a millimetre back and forth forever, which reads as
    // a shimmering line of particles along every surface.
    Knob<float> restThreshold_ {"Rest threshold", 0.05F, 0.0F, 1.0F, "%.3f u/s"};

    // TODO: scatter, to make a surface diffuse rather than a mirror. Wants an RNG, and
    // update() is const, so it waits on whether a behavior is allowed state.
};

struct Cull : public Behavior
{
    // TODO: accept shape in constructor?
    void update([[maybe_unused]] ParticlePool& pool, [[maybe_unused]] SceneObject const& object) const override
    {}

    void renderKnobs() override
    {
        ImGui::SeparatorText("Culling");
        options_.render();
        deadzone_.render();
    }

private:
    std::array<char const*, 2> selectionOptions_ {
        "Inside", "Outside"
    };

    Knob<char const*> options_ {"Culling Face", selectionOptions_, 1};
    Knob<float> deadzone_ {"Deadzone", 1.0F, -5.0F, 5.0F, "%.3f"};
};


#endif // YARR_LOGIC_BEHAVIOR_HPP
