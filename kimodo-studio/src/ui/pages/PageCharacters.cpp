#include "ui/pages/PageCharacters.h"
#include "animation/AnimationPlayer.h"
#include "character/CharacterLibrary.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UIHelpers.h"
#include "utils/AppPaths.h"
#include "utils/FileDialog.h"

#include <cstdio>
#include <string>

namespace studio
{

namespace
{

void draw_card_header(const char* title)
{
    ImGui::TextDisabled("%s", title);
    ImGui::Spacing();
}

void begin_card(const char* id, float height = 0.0f)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.12f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::BeginChild(id, ImVec2(0, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
}

void end_card()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
}

} // namespace

void PageCharacters::Draw(AppState& state, CharacterLibrary& chars, Viewport& viewport, AnimationPlayer* player)
{
    // =========================================================================
    // 1. TOP TAB BAR: Animation | Character | Inspector (matching Image 2)
    // =========================================================================
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float tab_w = avail_w / 3.0f;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto draw_workspace_tab = [&](const char* label, int tab_idx) {
        bool is_active = (state.right_panel_tab == tab_idx);
        ImVec2 p0 = ImGui::GetCursorScreenPos();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.14f, 0.18f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.14f, 0.18f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_Text, is_active ? UIStyle::accent : UIStyle::text_muted);

        if (ImGui::Button(label, ImVec2(tab_w - 4.0f, 32.0f)))
        {
            state.right_panel_tab = tab_idx;
        }
        ImGui::PopStyleColor(4);

        if (is_active)
        {
            // Green bottom underline bar matching Image 2
            draw_list->AddRectFilled(ImVec2(p0.x + 8.0f, p0.y + 30.0f), ImVec2(p0.x + tab_w - 12.0f, p0.y + 32.5f),
                                     ImGui::GetColorU32(UIStyle::accent), 1.0f);
        }
    };

    draw_workspace_tab("Animation", 0);
    ImGui::SameLine(0, 4);
    draw_workspace_tab("Character", 1);
    ImGui::SameLine(0, 4);
    draw_workspace_tab("Inspector", 2);

    ImGui::Spacing();
    ImGui::Spacing();

    // =========================================================================
    // 2. TAB 1: CHARACTER (Selected Character, Mapping, Transform, Display)
    // =========================================================================
    if (state.right_panel_tab == 1)
    {
        CharacterAsset* active = chars.GetActiveAsset();
        CharacterEntry cur_entry;
        bool has_entry = chars.FindEntry(chars.GetActiveId(), cur_entry);

        static bool show_change_char_modal = false;
        static bool show_import_modal = false;
        static bool show_mapping_modal = false;

        // -------------------------------------------------------------
        // CARD 1: SELECTED CHARACTER
        // -------------------------------------------------------------
        begin_card("##CardSelectedCharacter", 162.0f);
        {
            draw_card_header("Selected Character");

            // Thumbnail box on the left
            float thumb_size = 64.0f;
            ImVec2 thumb_pos = ImGui::GetCursorScreenPos();
            draw_list->AddRectFilled(thumb_pos, ImVec2(thumb_pos.x + thumb_size, thumb_pos.y + thumb_size),
                                     IM_COL32(16, 18, 24, 255), 6.0f);
            draw_list->AddRect(thumb_pos, ImVec2(thumb_pos.x + thumb_size, thumb_pos.y + thumb_size),
                               IM_COL32(40, 46, 58, 200), 6.0f);

            // Draw humanoid preview silhouette inside thumbnail box
            float cx = thumb_pos.x + thumb_size * 0.5f;
            float cy = thumb_pos.y + thumb_size * 0.5f;
            ImU32 sil_col = IM_COL32(180, 190, 205, 230);
            draw_list->AddCircleFilled(ImVec2(cx, cy - 18.0f), 5.0f, sil_col, 16);                           // Head
            draw_list->AddLine(ImVec2(cx, cy - 13.0f), ImVec2(cx, cy + 8.0f), sil_col, 3.5f);                // Spine
            draw_list->AddLine(ImVec2(cx - 12.0f, cy - 6.0f), ImVec2(cx + 12.0f, cy - 6.0f), sil_col, 2.5f); // Arms
            draw_list->AddLine(ImVec2(cx, cy + 8.0f), ImVec2(cx - 8.0f, cy + 24.0f), sil_col, 2.5f);         // Left Leg
            draw_list->AddLine(ImVec2(cx, cy + 8.0f), ImVec2(cx + 8.0f, cy + 24.0f), sil_col, 2.5f); // Right Leg

            ImGui::Dummy(ImVec2(thumb_size, thumb_size));
            ImGui::SameLine(0, 14.0f);

            // Character details on the right
            ImGui::BeginGroup();
            {
                std::string char_name = "SOMA Humanoid";
                if (active && active->IsLoaded() && !active->GetName().empty())
                {
                    char_name = active->GetName();
                }
                ImGui::TextColored(UIStyle::text, "%s", char_name.c_str());

                // Loaded status row with green dot
                ImGui::TextColored(UIStyle::accent, "%s", icons::kCheckCircle);
                ImGui::SameLine(0, 5);
                ImGui::TextColored(UIStyle::text, "Loaded");

                if (active && active->IsLoaded())
                {
                    ImGui::TextDisabled("Bones:   %zu", active->GetBones().size());
                    ImGui::TextDisabled("Status:  Ready");
                }
            }
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();

            // Two Buttons: [ Change Character ]  [ Import... ]
            float btn_w = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
            if (ImGui::Button("Change Character", ImVec2(btn_w, 30.0f)))
            {
                show_change_char_modal = true;
            }
            ImGui::SameLine(0, 8);
            if (ImGui::Button("Import...", ImVec2(btn_w, 30.0f)))
            {
                show_import_modal = true;
            }
        }
        end_card();

        // -------------------------------------------------------------
        // CARD 2: SKELETON MAPPING
        // -------------------------------------------------------------
        begin_card("##CardSkeletonMapping", 88.0f);
        {
            draw_card_header("Skeleton Mapping");

            size_t mapped_count = 30;
            size_t total_bones = 30;
            if (active && active->IsLoaded())
            {
                total_bones = active->GetBones().size();
                if (has_entry)
                {
                    mapped_count = 0;
                    for (const auto& [bone, joint] : cur_entry.mapping)
                    {
                        if (!joint.empty() && joint != "(none)")
                            mapped_count++;
                    }
                }
            }

            // Green dot + 30 / 30 joints mapped
            ImGui::TextColored(UIStyle::accent, "%s", icons::kCheckCircle);
            ImGui::SameLine(0, 6);
            ImGui::TextColored(UIStyle::text, "%zu / %zu joints mapped", mapped_count, total_bones);

            ImGui::Spacing();
            float btn_w = ImGui::GetContentRegionAvail().x - 38.0f;
            if (ImGui::Button("Edit Mapping", ImVec2(btn_w, 30.0f)))
            {
                show_mapping_modal = true;
            }
            ImGui::SameLine(0, 6);
            if (ImGui::Button(icons::kSettings, ImVec2(32.0f, 30.0f)))
            {
                show_mapping_modal = true;
            }
        }
        end_card();

        // -------------------------------------------------------------
        // CARD 3: MODEL TRANSFORM
        // -------------------------------------------------------------
        begin_card("##CardModelTransform", 184.0f);
        {
            draw_card_header("Model Transform");

            const float label_w = 68.0f;
            const float item_w = (ImGui::GetContentRegionAvail().x - label_w - 12.0f) / 3.0f;

            // Row 1: Position
            ui::DrawPropertyRow(
                "Position",
                [&](float) {
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##PosX", &state.model_position.x, 0.02f, -100.0f, 100.0f, "%.2f");
                    ImGui::SameLine(0, 4);
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##PosY", &state.model_position.y, 0.02f, -100.0f, 100.0f, "%.2f");
                    ImGui::SameLine(0, 4);
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##PosZ", &state.model_position.z, 0.02f, -100.0f, 100.0f, "%.2f");
                },
                label_w);

            // Row 2: Rotation
            ImGui::Spacing();
            ui::DrawPropertyRow(
                "Rotation",
                [&](float) {
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##RotX", &state.model_rotation.x, 1.0f, -360.0f, 360.0f, "%.2f");
                    ImGui::SameLine(0, 4);
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##RotY", &state.model_rotation.y, 1.0f, -360.0f, 360.0f, "%.2f");
                    ImGui::SameLine(0, 4);
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##RotZ", &state.model_rotation.z, 1.0f, -360.0f, 360.0f, "%.2f");
                },
                label_w);

            // Row 3: Scale
            ImGui::Spacing();
            ui::DrawPropertyRow(
                "Scale",
                [&](float) {
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##SclX", &state.model_scale.x, 0.01f, 0.01f, 50.0f, "%.2f");
                    ImGui::SameLine(0, 4);
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##SclY", &state.model_scale.y, 0.01f, 0.01f, 50.0f, "%.2f");
                    ImGui::SameLine(0, 4);
                    ImGui::SetNextItemWidth(item_w);
                    ImGui::DragFloat("##SclZ", &state.model_scale.z, 0.01f, 0.01f, 50.0f, "%.2f");
                },
                label_w);

            ImGui::Spacing();
            ImGui::Spacing();
            if (ImGui::Button("Reset Transform", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f)))
            {
                state.model_position = {0.0f, 0.0f, 0.0f};
                state.model_rotation = {0.0f, 0.0f, 0.0f};
                state.model_scale = {1.0f, 1.0f, 1.0f};
            }
        }
        end_card();

        // -------------------------------------------------------------
        // CARD 4: DISPLAY OPTIONS
        // -------------------------------------------------------------
        begin_card("##CardDisplayOptions", 218.0f);
        {
            draw_card_header("Display Options");

            // Quick mode selector: [ Character ]  [ Skeleton ]  [ Both ]
            const float mode_btn_w = (ImGui::GetContentRegionAvail().x - 8.0f) / 3.0f;
            const float mode_btn_h = 26.0f;

            bool is_char_only = state.show_character && !state.show_skeleton;
            bool is_skel_only = !state.show_character && state.show_skeleton;
            bool is_both = state.show_character && state.show_skeleton;

            auto draw_mode_btn = [&](const char* label, bool active) -> bool {
                if (active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.85f, 0.44f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.28f, 0.9f));
                    ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::text_muted);
                }
                bool clicked = ImGui::Button(label, ImVec2(mode_btn_w, mode_btn_h));
                ImGui::PopStyleColor(3);
                return clicked;
            };

            if (draw_mode_btn("Character", is_char_only))
            {
                state.show_character = true;
                state.show_skeleton = false;
            }
            ImGui::SameLine(0, 4);
            if (draw_mode_btn("Skeleton", is_skel_only))
            {
                state.show_character = false;
                state.show_skeleton = true;
            }
            ImGui::SameLine(0, 4);
            if (draw_mode_btn("Both", is_both))
            {
                state.show_character = true;
                state.show_skeleton = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_CheckMark, UIStyle::accent);

            ImGui::Checkbox("Show Character Mesh", &state.show_character);
            ImGui::Spacing();
            ImGui::Checkbox("Show Skeleton", &state.show_skeleton);
            ImGui::Spacing();
            ImGui::Checkbox("Show Joint Names", &state.show_joint_names);
            ImGui::Spacing();
            ImGui::Checkbox("Show Bone Names", &state.show_bone_names);
            ImGui::Spacing();
            ImGui::Checkbox("Wireframe", &state.show_wireframe);

            ImGui::PopStyleColor();
        }
        end_card();

        // -------------------------------------------------------------
        // MODALS: Change Character / Import GLB / Edit Mapping
        // -------------------------------------------------------------
        if (show_change_char_modal)
        {
            ImGui::OpenPopup("Select Character##Modal");
        }
        if (ImGui::BeginPopupModal("Select Character##Modal", &show_change_char_modal,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(UIStyle::accent, "%s Select Character", icons::kUser);
            ImGui::Separator();
            ImGui::Spacing();

            for (const auto& entry : chars.GetEntries())
            {
                bool is_selected = (entry.id == chars.GetActiveId());
                if (ImGui::Selectable(entry.name.c_str(), is_selected, 0, ImVec2(280, 28)))
                {
                    chars.SelectCharacter(entry.id);
                    state.active_character_id = entry.id;
                    viewport.SetCharacterAsset(chars.GetActiveAsset());
                    show_change_char_modal = false;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(80, 28)))
            {
                show_change_char_modal = false;
            }
            ImGui::EndPopup();
        }

        if (show_import_modal)
        {
            ImGui::OpenPopup("Import GLB Character##Modal");
        }
        if (ImGui::BeginPopupModal("Import GLB Character##Modal", &show_import_modal,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(UIStyle::accent, "%s Import 3D Character (GLB/glTF)", icons::kFolder);
            ImGui::Separator();
            ImGui::Spacing();

            static char import_path_buf[512] = "";
            ImGui::Text("File Path:");
            ImGui::SetNextItemWidth(360);
            ImGui::InputTextWithHint("##ImportModalPath", "e.g. assets/characters/model.glb", import_path_buf,
                                     sizeof(import_path_buf));
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_FOLDER " Browse...", ImVec2(110, 0)))
            {
                std::string start_dir = AppPaths::DefaultCharactersDir().string();
                std::string picked;
                if (FileDialog::OpenFile("glb,gltf", start_dir.c_str(), picked))
                {
                    strncpy_s(import_path_buf, sizeof(import_path_buf), picked.c_str(), _TRUNCATE);
                }
            }

            ImGui::Spacing();
            if (ImGui::Button("Import", ImVec2(100, 28)))
            {
                std::string err;
                if (chars.ImportCharacter(import_path_buf, err))
                {
                    viewport.SetCharacterAsset(chars.GetActiveAsset());
                    show_import_modal = false;
                    import_path_buf[0] = '\0';
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 28)))
            {
                show_import_modal = false;
            }
            ImGui::EndPopup();
        }

        if (show_mapping_modal)
        {
            ImGui::OpenPopup("Edit Skeleton Mapping##Modal");
        }
        if (ImGui::BeginPopupModal("Edit Skeleton Mapping##Modal", &show_mapping_modal,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(UIStyle::accent, "%s Skeleton Bone Mapping", icons::kRetarget);
            ImGui::TextDisabled("Map character rig bones to SOMA humanoid joints");
            ImGui::Separator();
            ImGui::Spacing();

            if (active && active->IsLoaded() && has_entry)
            {
                if (ImGui::BeginTable("##ModalBoneMappingTable", 3,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                                      ImVec2(460, 320)))
                {
                    ImGui::TableSetupColumn("Character Bone", ImGuiTableColumnFlags_WidthStretch, 0.45f);
                    ImGui::TableSetupColumn("Target Joint", ImGuiTableColumnFlags_WidthStretch, 0.45f);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableHeadersRow();

                    const auto& bones = active->GetBones();
                    for (size_t b = 0; b < bones.size(); ++b)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", bones[b].name.c_str());

                        ImGui::TableSetColumnIndex(1);
                        auto it = cur_entry.mapping.find(bones[b].name);
                        std::string mapped_joint = (it != cur_entry.mapping.end()) ? it->second : "(none)";
                        ImGui::Text("%s", mapped_joint.c_str());

                        ImGui::TableSetColumnIndex(2);
                        if (mapped_joint != "(none)" && !mapped_joint.empty())
                        {
                            ImGui::TextColored(UIStyle::green, "%s", icons::kCheck);
                        }
                        else
                        {
                            ImGui::TextDisabled("-");
                        }
                    }
                    ImGui::EndTable();
                }
            }
            else
            {
                ImGui::TextDisabled("Standard SOMA Humanoid: 30 / 30 joints auto-mapped 1:1.");
            }

            ImGui::Spacing();
            if (ImGui::Button("Done", ImVec2(100, 28)))
            {
                show_mapping_modal = false;
            }
            ImGui::EndPopup();
        }
    }

    // =========================================================================
    // 3. TAB 0: ANIMATION
    // =========================================================================
    else if (state.right_panel_tab == 0)
    {
        begin_card("##CardAnimationDetails");
        {
            draw_card_header("Active Animation");

            std::string title = "A person eating an apple";
            if (player && player->HasAnimation() && !player->GetAnimation().skeleton_name.empty())
            {
                title = "Active Motion (" + player->GetAnimation().skeleton_name + ")";
            }
            ImGui::TextColored(UIStyle::text, "%s", title.c_str());
            ImGui::Spacing();

            float dur = player ? player->GetDuration() : 4.0f;
            int fr = player ? player->GetTotalFrames() : 120;
            float fps = player ? player->GetFps() : 30.0f;

            ImGui::TextDisabled("Duration:  %.2f seconds", dur);
            ImGui::TextDisabled("Frames:    %d frames", fr);
            ImGui::TextDisabled("Framerate: %.1f FPS", fps);
            ImGui::TextDisabled("Joints:    30 joints (soma30)");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (player)
            {
                ImGui::TextDisabled("Current Frame: %03d / %03d", player->Frame(), fr);
                ImGui::TextDisabled("Current Time:  %.2f / %.2f s", player->GetTime(), dur);

                ImGui::Spacing();
                if (ImGui::Button(player->IsPlaying() ? "Pause Animation" : "Play Animation",
                                  ImVec2(ImGui::GetContentRegionAvail().x, 30.0f)))
                {
                    player->TogglePlay();
                }
            }
        }
        end_card();
    }

    // =========================================================================
    // 4. TAB 2: INSPECTOR (Mesh & Rig Validation)
    // =========================================================================
    else if (state.right_panel_tab == 2)
    {
        CharacterAsset* active = chars.GetActiveAsset();
        begin_card("##CardMeshInspector");
        {
            draw_card_header("Mesh & Rig Validation");

            auto draw_check = [](const char* label, bool ok, const std::string& desc = "") {
                if (ok)
                {
                    ImGui::TextColored(UIStyle::green, "%s %s", icons::kCheck, label);
                }
                else
                {
                    ImGui::TextColored(UIStyle::red, "%s %s", icons::kClose, label);
                }
                if (!desc.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", desc.c_str());
                }
            };

            if (active && active->IsLoaded())
            {
                const auto& rep = active->GetValidationReport();
                draw_check("Geometry", rep.has_mesh, std::to_string(rep.vertex_count) + " verts");
                draw_check("Skeleton", rep.has_skeleton, std::to_string(rep.bone_count) + " bones");
                draw_check("Skinning", rep.has_skin, rep.has_skin ? "Skin matrices valid" : "No skin");
                draw_check("Weights", rep.valid_weights, rep.valid_weights ? "Normalized" : "Check weights");

                char h_buf[32];
                std::snprintf(h_buf, sizeof(h_buf), "Height: %.2fm", rep.height);
                draw_check("Rest Pose", rep.valid_rest_pose, h_buf);

                if (!rep.warnings.empty())
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Warnings: %zu", rep.warnings.size());
                }
            }
            else
            {
                ImGui::TextDisabled("No active character asset loaded.");
            }
        }
        end_card();
    }
}

} // namespace studio
