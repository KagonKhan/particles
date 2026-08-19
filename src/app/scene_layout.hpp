#ifndef YARR_SCENE_LAYOUT_HPP
#define YARR_SCENE_LAYOUT_HPP

#include "logic/objects/attractor.hpp"
#include "logic/scene.hpp"
#include "logic/scene_object.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <vector>

class SceneView
{
public:
    // Non-const: the panel on the right edits the object it is shown, so it needs the live
    // one rather than a copy that the next frame throws away.
    void render(Scene& scene)
    {
        std::vector<SceneObject*> const objects = scene.sceneObjects();

        if (objects.empty()) {
            return;
        }

        selectedItem_ = std::min(selectedItem_, objects.size() - 1);

        ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Scene View")) {
            // Left
            {
                ImGui::BeginChild("left pane", ImVec2(150, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

                for (std::size_t id = 0; id < objects.size(); ++id) {
                    ImGui::PushID(static_cast<int>(id));

                    if (ImGui::Selectable(
                            objects[id]->name.c_str(),
                            (selectedItem_ == id),
                            ImGuiSelectableFlags_SelectOnNav)) {
                        selectedItem_ = id;
                    }

                    ImGui::PopID();
                }

                ImGui::EndChild();
            }
            ImGui::SameLine();

            // Right
            {
                SceneObject& object = *objects[selectedItem_];

                ImGui::BeginGroup();
                ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us
                ImGui::Text("%s", object.name.c_str());
                ImGui::Separator();

                SceneObjectView {}(object);

                ImGui::SeparatorText("Settings");

                if (object.attachedType == SceneObject::AttachedType::EMITTER) {
                    object.getAttachment<Emitter>()->renderSettings(*scene.pool());
                }

                if (object.attachedType == SceneObject::AttachedType::ATTRACTOR) {
                    object.getAttachment<Attractor>()->renderSettings();
                }

                ImGui::EndChild();

                ImGui::EndGroup();
            }
        }

        ImGui::End();
    }

private:
    inline static std::size_t selectedItem_ {0};
};

#endif // YARR_SCENE_LAYOUT_HPP
