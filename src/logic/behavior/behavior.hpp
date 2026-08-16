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

constexpr float CONTACT_SKIN = 1e-4F;


struct Behavior
{
public:
    Behavior()          = default;
    virtual ~Behavior() = default;

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
    }

private:
    template <typename ShapeT>
    void resolve(ShapeT shape, ParticleChunk chunk, glm::vec2 origin) const
    {
        for (std::size_t i {}; i < chunk.size(); ++i) {
            glm::vec2&      position = chunk.positions[i];
            glm::vec2 const local    = position - origin;

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

            if (approach >= 0.0F) {
                continue;
            }

            glm::vec2 const tangential = velocity - (approach * hit.normal);
            velocity = tangential - (approach * bounceFactor_.get()) * hit.normal;
        }
    }

    Knob<float> bounceFactor_ {"Bounce Factor", 1.0F, 0.0F, 2.0F, "%.3f"};

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
