#include "ui/pages/PageRetarget.h"
#include "animation/AnimationPlayer.h"
#include "imgui.h"
#include "library/AnimationLibrary.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"

namespace studio {

void SPageRetarget::Draw(FAppState& state, FAnimationLibrary& library,
                        FAnimationPlayer& player, SToasts& toasts) {
    (void)state;
    ImGui::TextColored(FUIStyle::accent, "%s Skeleton Retargeting (Generic & Blender)", icons::kRetarget);
    ImGui::TextDisabled("Retarget generic humanoid motion to standard DCC skeletons");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Unreal Engine Native Workflow Note Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.9f));
    ImGui::BeginChild("##UeWorkflowCard", ImVec2(0, 85), true);
    {
        ImGui::TextColored(FUIStyle::accent, "%s Unreal Engine Pipeline:", icons::kInfo);
        ImGui::Text("Kimodo -> Generic Humanoid BVH -> Unreal IK Rig -> Unreal IK Retargeter -> Manny");
        ImGui::TextDisabled("For Unreal Engine, export a BVH file from the Export page. Use UE5's native IK Retargeter for perfect results.");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Target Profile Selector
    const auto& profiles = targetProfiles();
    static std::string selectedProfileId = "blender-generic";

    ImGui::Text("Target Skeleton Profile:");
    for (const auto& prof : profiles) {
        bool sel = (prof.id == selectedProfileId);
        if (ImGui::RadioButton(prof.name.c_str(), sel)) {
            selectedProfileId = prof.id;
        }
        ImGui::SameLine(0, 16);
    }
    ImGui::NewLine();

    const FSkeletonProfile* targetProfile = FindProfile(selectedProfileId);
    if (!targetProfile) return;

    // 2. Source Animation Selector
    const auto& libEntries = library.GetEntries();
    ImGui::Text("Source Animation from Library:");
    if (libEntries.empty()) {
        ImGui::TextDisabled("No animations in library. Generate an animation first.");
    } else {
        static int selectedAnimIdx = 0;
        if (selectedAnimIdx >= static_cast<int>(libEntries.size())) selectedAnimIdx = 0;

        std::string previewName = libEntries[selectedAnimIdx].prompt;
        if (ImGui::BeginCombo("##SourceAnimCombo", previewName.c_str())) {
            for (size_t i = 0; i < libEntries.size(); ++i) {
                bool isSel = (static_cast<int>(i) == selectedAnimIdx);
                if (ImGui::Selectable(libEntries[i].prompt.c_str(), isSel)) {
                    selectedAnimIdx = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }

        // 3. Mapping Table
        static FBoneMap activeMap = FRetargeter::autoMap(*targetProfile);
        ImGui::Spacing();
        ImGui::TextColored(FUIStyle::text, "Joint Mapping (%llu target joints):",
                           static_cast<unsigned long long>(targetProfile->joints.size()));

        if (ImGui::BeginTable("##RetargetMappingTable", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 200))) {
            ImGui::TableSetupColumn("Target Joint", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Source Joint", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableHeadersRow();

            for (const auto& j : targetProfile->joints) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", j.c_str());

                ImGui::TableSetColumnIndex(1);
                auto it = activeMap.find(j);
                std::string cur = (it != activeMap.end()) ? it->second : "(none)";
                ImGui::Text("%s", cur.c_str());
            }
            ImGui::EndTable();
        }

        // 4. Retarget Action
        ImGui::Spacing();
        static float rootScale = 1.0f;
        ImGui::SliderFloat("Root Scale", &rootScale, 0.1f, 5.0f, "%.2fx");

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, FUIStyle::accent);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));

        if (ImGui::Button(ICON_FA_RETARGET "  Execute Retarget", ImVec2(-1, 40))) {
            FAnimation srcAnim;
            if (library.loadAnimation(libEntries[selectedAnimIdx], srcAnim)) {
                FAnimation retargeted;
                std::string err;
                FRetargetReport report;
                FRetargeter::Options ropts;
                ropts.rootScale = rootScale;
                if (FRetargeter::retarget(srcAnim, *targetProfile, activeMap, ropts,
                                        retargeted, err, &report)) {
                    player.load(retargeted);
                    toasts.Push("Retargeting complete: " + std::to_string(report.mappedCount) + " joints mapped",
                                EToastKind::Success);
                } else {
                    toasts.Push("Retargeting failed: " + err, EToastKind::Error);
                }
            } else {
                toasts.Push("Failed to load source animation", EToastKind::Error);
            }
        }
        ImGui::PopStyleColor(2);
    }
}

} // namespace studio
