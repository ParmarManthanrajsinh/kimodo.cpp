#include "ui/pages/PageHome.h"
#include "character/CharacterLibrary.h"
#include "imgui.h"
#include "library/AnimationLibrary.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio
{

void PageHome::Draw(AppState& state, AnimationLibrary& lib, CharacterLibrary& chars)
{
    ImGui::TextColored(UIStyle::accent, "%s Welcome to Kimodo Studio", icons::kKimodo);
    ImGui::TextDisabled("C++23 AI Character Motion Generation & Animation Workstation");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Quick Stats Overview
    ImGui::BeginGroup();
    {
        ImGui::TextColored(UIStyle::text, "%s Pipeline Overview", icons::kPlay);
        ImGui::BulletText("Kimodo Generation -> SOMA30 Model Inference");
        ImGui::BulletText("Animation Library -> Real-time Forward Kinematics");
        ImGui::BulletText("Character Preview -> GLB Humanoid Rig + GPU Vertex Skinning");
        ImGui::BulletText("Unreal Export -> Generic Humanoid BVH -> Unreal IK Rig / Retargeter");
        ImGui::BulletText("Blender Export -> Generic Humanoid GLB / Local Retargeting");
    }
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Quick Actions
    ImGui::TextColored(UIStyle::text, "Quick Actions");
    ImGui::Spacing();

    if (ImGui::Button(ICON_FA_GENERATE "  New Motion Generation", ImVec2(240, 42)))
    {
        state.screen = Screen::Generate;
        state.last_tool_screen = Screen::Generate;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_USER "  Manage 3D Characters", ImVec2(240, 42)))
    {
        state.screen = Screen::Characters;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_EXPORT "  Export for Unreal / Blender", ImVec2(240, 42)))
    {
        state.screen = Screen::Export;
        state.last_tool_screen = Screen::Export;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Active Assets summary
    ImGui::TextDisabled("Loaded Assets: %llu Animations in Library | %llu Characters registered",
                        static_cast<unsigned long long>(lib.GetEntries().size()),
                        static_cast<unsigned long long>(chars.GetEntries().size()));
}

} // namespace studio
