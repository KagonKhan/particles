#ifndef YARR_LOGIC_BOUNDARY_HPP
#define YARR_LOGIC_BOUNDARY_HPP

#include "logic/behavior/behavior.hpp"
#include "logic/particle_pool.hpp"
#include "logic/scene_object.hpp"

#include <imgui.h>


// A shape with something to say about the particles that reach it. One hardcoded behavior
// for now — the point is to have somewhere to hang the bounce while it is being tuned, and
// the shape combo already lets every case be tried against it.
//
// TODO: a std::vector<std::unique_ptr<Behavior>> here, once there is more than one.
class Boundary
{
public:
    void apply(ParticleChunk chunk) const { behavior_.apply(chunk, object_); }

    void setPosition(glm::vec2 position) noexcept { object_.transform.position = position; }

    void renderSettings()
    {
        ImGui::Begin("Boundary Settings");

        behavior_.renderKnobs();

        ImGui::SeparatorText("Body");

        renderSceneObjectSettings(object_);

        ImGui::End();
    }

    [[nodiscard]] bool               isVisible() const noexcept { return object_.visible; }
    [[nodiscard]] SceneObject const& object() const noexcept    { return object_; }

private:
    Bounce behavior_;

    // A frame, since that is the one shape that is a room rather than an obstacle and so
    // shows a bounce working without the particles simply leaving.
    SceneObject object_ {
        .transform = {},
        .shape     = Frame {},
        .height    = 0.25F,
        .color     = {0.80F, 0.55F, 0.30F, 1.0F},
        .visible   = true,
    };
};

#endif // YARR_LOGIC_BOUNDARY_HPP
