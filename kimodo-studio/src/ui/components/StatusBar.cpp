#include "ui/components/StatusBar.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {

void StatusBar::draw(AppState& state, Viewport& viewport) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - UIStyle::statusH));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, UIStyle::statusH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIStyle::bg);

    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        ImGui::TextDisabled("Kimodo Studio | C++23 Native Workstation");

        ImGui::SameLine(0, 32);
        ImGui::TextDisabled("View:");
        ImGui::SameLine(0, 8);

        // Segmented buttons: [ Character ] [ Skeleton ] [ Both ]
        auto drawModeBtn = [&](const char* label, ViewportMode mode, bool charVis, bool skelVis) {
            bool active = (state.viewportMode == mode);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::card);
                ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::textMuted);
            }
            if (ImGui::SmallButton(label)) {
                state.viewportMode = mode;
                viewport.setCharacter(charVis);
                viewport.setSkeleton(skelVis);
            }
            ImGui::PopStyleColor(2);
        };

        drawModeBtn("Character", ViewportMode::Character, true, false);
        ImGui::SameLine(0, 4);
        drawModeBtn("Skeleton", ViewportMode::Skeleton, false, true);
        ImGui::SameLine(0, 4);
        drawModeBtn("Both", ViewportMode::Both, true, true);

        ImGui::SameLine(0, 24);

        // Toggle buttons: Wireframe, Bones, Grid
        auto drawToggle = [&](const char* icon, const char* label, bool active, auto setter) {
            std::string btnText = std::string(icon) + " " + label;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.35f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::green);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::card);
                ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::textMuted);
            }
            if (ImGui::SmallButton(btnText.c_str())) {
                setter(!active);
            }
            ImGui::PopStyleColor(2);
        };

        drawToggle(icons::kGrid, "Wireframe", viewport.showWireframe(), [&](bool v) {
            viewport.setWireframe(v);
            state.showWireframe = v;
        });

        ImGui::SameLine(0, 8);
        drawToggle(icons::kSkeleton, "Bones", viewport.showBoneNames(), [&](bool v) {
            viewport.setBoneNames(v);
            state.showBoneNames = v;
        });

        ImGui::SameLine(0, 8);
        drawToggle(icons::kGrid, "Grid", viewport.showGrid(), [&](bool v) {
            viewport.setGrid(v);
            state.showGrid = v;
        });

        // Right side badge
        ImGui::SameLine(ImGui::GetWindowWidth() - 170);
        ImGui::TextColored(UIStyle::green, "%s Tensor Accelerated", icons::kCheck);
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace studio
