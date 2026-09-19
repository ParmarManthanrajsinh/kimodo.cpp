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

void PageRetarget::Draw(AppState& state, AnimationLibrary& library, AnimationPlayer& player, Toasts& toasts) {
    (void)state;
    ImGui::TextColored(UIStyle::accent, "%s Skeleton Retargeting (Generic & Blender)", icons::kRetarget);
    ImGui::TextDisabled("Retarget generic humanoid motion to standard DCC skeletons");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Unreal Engine Native Workflow Note Card
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.9f));
    ImGui::BeginChild("##UeWorkflowCard", ImVec2(0, 85), true);
    {
        ImGui::TextColored(UIStyle::accent, "%s Unreal Engine Pipeline:", icons::kInfo);
        ImGui::Text("Kimodo -> Generic Humanoid BVH -> Unreal IK Rig -> Unreal IK Retargeter -> Manny");
        ImGui::TextDisabled("For Unreal Engine, export a BVH file from the Export page. Use UE5's native IK Retargeter "
                            "for perfect results.");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Target Profile Selector
    const auto& profiles = target_profiles();
    static std::string selected_profile_id = "blender-generic";

    ImGui::Text("Target Skeleton Profile:");
    for (const auto& prof : profiles) {
        bool sel = (prof.id == selected_profile_id);
        if (ImGui::RadioButton(prof.name.c_str(), sel)) {
            selected_profile_id = prof.id;
        }
        ImGui::SameLine(0, 16);
    }
    ImGui::NewLine();

    const SkeletonProfile* target_profile = FindProfile(selected_profile_id);
    if (!target_profile)
        return;

    // 2. Source Animation Selector
    const auto& lib_entries = library.GetEntries();
    ImGui::Text("Source Animation from Library:");
    if (lib_entries.empty()) {
        ImGui::TextDisabled("No animations in library. Generate an animation first.");
    } else {
        static int selected_anim_idx = 0;
        if (selected_anim_idx >= static_cast<int>(lib_entries.size()))
            selected_anim_idx = 0;

        std::string previewName = lib_entries[selected_anim_idx].prompt;
        if (ImGui::BeginCombo("##SourceAnimCombo", previewName.c_str())) {
            for (size_t i = 0; i < lib_entries.size(); ++i) {
                bool is_sel = (static_cast<int>(i) == selected_anim_idx);
                if (ImGui::Selectable(lib_entries[i].prompt.c_str(), is_sel)) {
                    selected_anim_idx = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }

        // 3. Mapping Table
        static BoneMap active_map = Retargeter::AutoMap(*target_profile);
        ImGui::Spacing();
        ImGui::TextColored(UIStyle::text, "Joint Mapping (%llu target joints):",
                           static_cast<unsigned long long>(target_profile->joints.size()));

        if (ImGui::BeginTable("##RetargetMappingTable", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 200))) {
            ImGui::TableSetupColumn("Target Joint", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Source Joint", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableHeadersRow();

            for (const auto& j : target_profile->joints) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", j.c_str());

                ImGui::TableSetColumnIndex(1);
                auto it = active_map.find(j);
                std::string cur = (it != active_map.end()) ? it->second : "(none)";
                ImGui::Text("%s", cur.c_str());
            }
            ImGui::EndTable();
        }

        // 4. Retarget Action
        ImGui::Spacing();
        static float root_scale = 1.0f;
        ImGui::SliderFloat("Root Scale", &root_scale, 0.1f, 5.0f, "%.2fx");

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));

        if (ImGui::Button(ICON_FA_RETARGET "  Execute Retarget", ImVec2(-1, 40))) {
            Animation src_anim;
            if (library.LoadAnimation(lib_entries[selected_anim_idx], src_anim)) {
                Animation retargeted;
                std::string err;
                RetargetReport report;
                Retargeter::Options ropts;
                ropts.root_scale = root_scale;
                if (Retargeter::retarget(src_anim, *target_profile, active_map, ropts, retargeted, err, &report)) {
                    player.Load(retargeted);
                    toasts.Push("Retargeting complete: " + std::to_string(report.mapped_count) + " joints mapped",
                                ToastKind::Success);
                } else {
                    toasts.Push("Retargeting failed: " + err, ToastKind::Error);
                }
            } else {
                toasts.Push("Failed to load source animation", ToastKind::Error);
            }
        }
        ImGui::PopStyleColor(2);
    }
}

} // namespace studio
