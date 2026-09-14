#include "ui/Theme.h"

#include "imgui.h"

namespace studio {

void Theme::apply() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.FrameRounding = 6.0f;
    s.FrameBorderSize = 1.0f;
    s.WindowBorderSize = 1.0f;
    s.ItemSpacing = ImVec2(8, 6);

    ImVec4* c = ImGui::GetStyle().Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.11f, 0.14f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.20f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.25f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.30f, 1.0f);
    c[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.13f, 0.16f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.25f, 0.45f, 0.85f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.52f, 0.95f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.38f, 0.75f, 1.0f);
    c[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.30f, 1.0f);
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.60f, 1.0f);
}

} // namespace studio
