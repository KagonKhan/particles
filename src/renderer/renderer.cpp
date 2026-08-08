#include "renderer.hpp"

#include <imgui.h>

#include <random>
#include <vector>


void updateWithNoise(Image& image)
{
    auto size = image.size();

    std::vector<unsigned char> pixels(size.x * size.y * 4);

    static std::mt19937                                gen(std::random_device {}());
    static std::uniform_int_distribution<unsigned int> dist(0, 255);

    for (auto& p : pixels) {
        p = static_cast<unsigned char>(rand() & 0xFF);
    }

    image.update(pixels.data());
}

Renderer::Renderer()
{
    updateWithNoise(image);
}

void Renderer::render()
{
    ImGui::Begin("Renderer options");
    ImGui::Text("pointer = %x", image.id());
    ImGui::Text("size = %d x %d", (int)image.size().x, (int)image.size().y);

    if (ImGui::Checkbox("Generate noise", &generateNoise_) || generateNoise_) {
        updateWithNoise(image);
    }

    ImGui::End();


    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin(
        "Render",
        nullptr,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 avail = ImGui::GetContentRegionAvail();

    image.resize(ImGui::GetWindowContentRegionMax());
    ImGui::Image((ImTextureID)(intptr_t)image.id(), image.size());
    ImGui::End();
    ImGui::PopStyleVar(1);
}
