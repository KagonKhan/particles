#ifndef YARR_LOGIC_BEHAVIOR_HPP
#define YARR_LOGIC_BEHAVIOR_HPP

#include "logic/particle_pool.hpp"
#include "logic/scene_object.hpp"
#include "utils/knob.hpp"

#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>
#include <imgui.h>

#include <array>
#include <cstddef>
#include <variant>

// How far off the surface a resolved particle is left. Landing it exactly on the outline
// lets rounding put it back inside, and it would spend every frame being pushed out again.
constexpr float CONTACT_SKIN = 1e-4F;


struct Behavior
{
public:
    Behavior()          = default;
    virtual ~Behavior() = default;

    // A chunk rather than the pool, so the scene can hand the same slice to every object
    // while it is still in cache. One virtual call per chunk — a thousandth of what calling
    // through this interface per particle would cost, and it buys back the dynamic
    // behavior list this is an interface for.
    virtual void apply(ParticleChunk chunk, SceneObject const& object) const = 0;
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
    //
    // The variant is opened once for the whole chunk, not once per particle: with the
    // alternative in hand the field and its gradient inline into the loop below, and the
    // broad phase there is a couple of compares rather than a call through the variant.
    void apply(ParticleChunk chunk, SceneObject const& object) const override
    {
        std::visit(
            [this, chunk, &object] (auto concrete) { resolve(concrete, chunk, object.transform.position); },
            object.shape);
    }

    void renderKnobs() override
    {
        ImGui::SeparatorText("Bouncing");
        bounceFactor_.render();
        friction_.render();
        restThreshold_.render();
    }

private:
    template <typename ShapeT>
    void resolve(ShapeT shape, ParticleChunk chunk, glm::vec2 origin) const
    {
        float const restitution = bounceFactor_.get();
        float const slide       = 1.0F - friction_.get();
        float const rest        = restThreshold_.get();

        for (std::size_t i {}; i < chunk.size(); ++i) {
            glm::vec2&      position = chunk.positions[i];
            glm::vec2 const local    = position - origin;

            // Most of a pool is nowhere near any one body on any one frame, and this is what
            // those particles cost — no square root, no gradient, no contact built.
            if (!mayContact(shape, local, CONTACT_SKIN)) {
                continue;
            }

            Contact const hit = contact(shape, local);

            if (hit.distance >= CONTACT_SKIN) {
                continue;
            }

            glm::vec2& velocity = chunk.velocities[i];

            position += hit.normal * (CONTACT_SKIN - hit.distance);

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
    void apply([[maybe_unused]] ParticleChunk chunk, [[maybe_unused]] SceneObject const& object) const override
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
