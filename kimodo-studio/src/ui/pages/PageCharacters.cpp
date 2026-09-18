#include "ui/pages/PageCharacters.h"
#include "animation/AnimationPlayer.h"
#include "character/CharacterLibrary.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UIHelpers.h"
#include "utils/FileDialog.h"
#include "utils/AppPaths.h"

#include <cstdio>
#include <string>

namespace studio {

namespace {

void drawCardHeader(const char* title) {
    ImGui::TextDisabled("%s", title);
    ImGui::Spacing();
}

void beginCard(const char* id, float height = 0.0f) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.12f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::BeginChild(id, ImVec2(0, height), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
}

void endCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
}

} // namespace

void SPageCharacters::Draw(FAppState& state, FCharacterLibrary& chars, FViewport& viewport,
                          FAnimationPlayer* player) {
    // =========================================================================
    // 1. TOP TAB BAR: Animation | Character | Inspector (matching Image 2)
    // =========================================================================
    const float availW = ImGui::GetContentRegionAvail().x;
    const float tabW = availW / 3.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto drawWorkspaceTab = [&](const char* label, int tabIdx) {
        bool isActive = (state.rightPanelTab == tabIdx);
        ImVec2 p0 = ImGui::GetCursorScreenPos();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.14f, 0.18f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.14f, 0.18f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_Text, isActive ? FUIStyle::accent : FUIStyle::textMuted);

        if (ImGui::Button(label, ImVec2(tabW - 4.0f, 32.0f))) {
            state.rightPanelTab = tabIdx;
        }
        ImGui::PopStyleColor(4);

        if (isActive) {
            // Green bottom underline bar matching Image 2
            drawList->AddRectFilled(ImVec2(p0.x + 8.0f, p0.y + 30.0f),
                                    ImVec2(p0.x + tabW - 12.0f, p0.y + 32.5f),
                                    ImGui::GetColorU32(FUIStyle::accent), 1.0f);
        }
    };

    drawWorkspaceTab("Animation", 0);
    ImGui::SameLine(0, 4);
    drawWorkspaceTab("Character", 1);
    ImGui::SameLine(0, 4);
    drawWorkspaceTab("Inspector", 2);

    ImGui::Spacing();
    ImGui::Spacing();

    // =========================================================================
    // 2. TAB 1: CHARACTER (Selected Character, Mapping, Transform, Display)
    // =========================================================================
    if (state.rightPanelTab == 1) {
        FCharacterAsset* active = chars.GetActiveAsset();
        FCharacterEntry curEntry;
        bool hasEntry = chars.FindEntry(chars.GetActiveId(), curEntry);

        static bool showChangeCharModal = false;
        static bool showImportModal = false;
        static bool showMappingModal = false;

        // -------------------------------------------------------------
        // CARD 1: SELECTED CHARACTER
        // -------------------------------------------------------------
        beginCard("##CardSelectedCharacter", 162.0f);
        {
            drawCardHeader("Selected Character");

            // Thumbnail box on the left
            float thumbSize = 64.0f;
            ImVec2 thumbPos = ImGui::GetCursorScreenPos();
            drawList->AddRectFilled(thumbPos, ImVec2(thumbPos.x + thumbSize, thumbPos.y + thumbSize),
                                    IM_COL32(16, 18, 24, 255), 6.0f);
            drawList->AddRect(thumbPos, ImVec2(thumbPos.x + thumbSize, thumbPos.y + thumbSize),
                              IM_COL32(40, 46, 58, 200), 6.0f);

            // Draw humanoid preview silhouette inside thumbnail box
            float cx = thumbPos.x + thumbSize * 0.5f;
            float cy = thumbPos.y + thumbSize * 0.5f;
            ImU32 silCol = IM_COL32(180, 190, 205, 230);
            drawList->AddCircleFilled(ImVec2(cx, cy - 18.0f), 5.0f, silCol, 16); // Head
            drawList->AddLine(ImVec2(cx, cy - 13.0f), ImVec2(cx, cy + 8.0f), silCol, 3.5f); // Spine
            drawList->AddLine(ImVec2(cx - 12.0f, cy - 6.0f), ImVec2(cx + 12.0f, cy - 6.0f), silCol, 2.5f); // Arms
            drawList->AddLine(ImVec2(cx, cy + 8.0f), ImVec2(cx - 8.0f, cy + 24.0f), silCol, 2.5f); // Left Leg
            drawList->AddLine(ImVec2(cx, cy + 8.0f), ImVec2(cx + 8.0f, cy + 24.0f), silCol, 2.5f); // Right Leg

            ImGui::Dummy(ImVec2(thumbSize, thumbSize));
            ImGui::SameLine(0, 14.0f);

            // Character details on the right
            ImGui::BeginGroup();
            {
                std::string charName = "SOMA Humanoid";
                if (active && active->IsLoaded() && !active->GetName().empty()) {
                    charName = active->GetName();
                }
                ImGui::TextColored(FUIStyle::text, "%s", charName.c_str());

                // Loaded status row with green dot
                ImGui::TextColored(FUIStyle::accent, "%s", icons::kCheckCircle);
                ImGui::SameLine(0, 5);
                ImGui::TextColored(FUIStyle::text, "Loaded");

                if (active && active->IsLoaded()) {
                    ImGui::TextDisabled("Bones:   %zu", active->GetBones().size());
                    ImGui::TextDisabled("Status:  Ready");
                }
            }
            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();

            // Two Buttons: [ Change Character ]  [ Import... ]
            float btnW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
            if (ImGui::Button("Change Character", ImVec2(btnW, 30.0f))) {
                showChangeCharModal = true;
            }
            ImGui::SameLine(0, 8);
            if (ImGui::Button("Import...", ImVec2(btnW, 30.0f))) {
                showImportModal = true;
            }
        }
        endCard();

        // -------------------------------------------------------------
        // CARD 2: SKELETON MAPPING
        // -------------------------------------------------------------
        beginCard("##CardSkeletonMapping", 88.0f);
        {
            drawCardHeader("Skeleton Mapping");

            size_t mappedCount = 30;
            size_t totalBones = 30;
            if (active && active->IsLoaded()) {
                totalBones = active->GetBones().size();
                if (hasEntry) {
                    mappedCount = 0;
                    for (const auto& [bone, joint] : curEntry.mapping) {
                        if (!joint.empty() && joint != "(none)") mappedCount++;
                    }
                }
            }

            // Green dot + 30 / 30 joints mapped
            ImGui::TextColored(FUIStyle::accent, "%s", icons::kCheckCircle);
            ImGui::SameLine(0, 6);
            ImGui::TextColored(FUIStyle::text, "%zu / %zu joints mapped", mappedCount, totalBones);

            ImGui::Spacing();
            float btnW = ImGui::GetContentRegionAvail().x - 38.0f;
            if (ImGui::Button("Edit Mapping", ImVec2(btnW, 30.0f))) {
                showMappingModal = true;
            }
            ImGui::SameLine(0, 6);
            if (ImGui::Button(icons::kSettings, ImVec2(32.0f, 30.0f))) {
                showMappingModal = true;
            }
        }
        endCard();

        // -------------------------------------------------------------
        // CARD 3: MODEL TRANSFORM
        // -------------------------------------------------------------
        beginCard("##CardModelTransform", 184.0f);
        {
            drawCardHeader("Model Transform");

            const float labelW = 68.0f;
            const float itemW = (ImGui::GetContentRegionAvail().x - labelW - 12.0f) / 3.0f;

            // Row 1: Position
            ui::DrawPropertyRow("Position", [&](float) {
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##PosX", &state.modelPosition.x, 0.02f, -100.0f, 100.0f, "%.2f");
                ImGui::SameLine(0, 4);
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##PosY", &state.modelPosition.y, 0.02f, -100.0f, 100.0f, "%.2f");
                ImGui::SameLine(0, 4);
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##PosZ", &state.modelPosition.z, 0.02f, -100.0f, 100.0f, "%.2f");
            }, labelW);

            // Row 2: Rotation
            ImGui::Spacing();
            ui::DrawPropertyRow("Rotation", [&](float) {
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##RotX", &state.modelRotation.x, 1.0f, -360.0f, 360.0f, "%.2f");
                ImGui::SameLine(0, 4);
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##RotY", &state.modelRotation.y, 1.0f, -360.0f, 360.0f, "%.2f");
                ImGui::SameLine(0, 4);
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##RotZ", &state.modelRotation.z, 1.0f, -360.0f, 360.0f, "%.2f");
            }, labelW);

            // Row 3: Scale
            ImGui::Spacing();
            ui::DrawPropertyRow("Scale", [&](float) {
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##SclX", &state.modelScale.x, 0.01f, 0.01f, 50.0f, "%.2f");
                ImGui::SameLine(0, 4);
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##SclY", &state.modelScale.y, 0.01f, 0.01f, 50.0f, "%.2f");
                ImGui::SameLine(0, 4);
                ImGui::SetNextItemWidth(itemW);
                ImGui::DragFloat("##SclZ", &state.modelScale.z, 0.01f, 0.01f, 50.0f, "%.2f");
            }, labelW);

            ImGui::Spacing();
            ImGui::Spacing();
            if (ImGui::Button("Reset Transform", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f))) {
                state.modelPosition = {0.0f, 0.0f, 0.0f};
                state.modelRotation = {0.0f, 0.0f, 0.0f};
                state.modelScale = {1.0f, 1.0f, 1.0f};
            }
        }
        endCard();

        // -------------------------------------------------------------
        // CARD 4: DISPLAY OPTIONS
        // -------------------------------------------------------------
        beginCard("##CardDisplayOptions", 218.0f);
        {
            drawCardHeader("Display Options");

            // Quick mode selector: [ Character ]  [ Skeleton ]  [ Both ]
            const float modeBtnW = (ImGui::GetContentRegionAvail().x - 8.0f) / 3.0f;
            const float modeBtnH = 26.0f;

            bool isCharOnly = state.showCharacter && !state.showSkeleton;
            bool isSkelOnly = !state.showCharacter && state.showSkeleton;
            bool isBoth = state.showCharacter && state.showSkeleton;

            auto drawModeBtn = [&](const char* label, bool active) -> bool {
                if (active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, FUIStyle::accent);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.85f, 0.44f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.28f, 0.9f));
                    ImGui::PushStyleColor(ImGuiCol_Text, FUIStyle::textMuted);
                }
                bool clicked = ImGui::Button(label, ImVec2(modeBtnW, modeBtnH));
                ImGui::PopStyleColor(3);
                return clicked;
            };

            if (drawModeBtn("Character", isCharOnly)) {
                state.showCharacter = true;
                state.showSkeleton = false;
            }
            ImGui::SameLine(0, 4);
            if (drawModeBtn("Skeleton", isSkelOnly)) {
                state.showCharacter = false;
                state.showSkeleton = true;
            }
            ImGui::SameLine(0, 4);
            if (drawModeBtn("Both", isBoth)) {
                state.showCharacter = true;
                state.showSkeleton = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_CheckMark, FUIStyle::accent);

            ImGui::Checkbox("Show Character Mesh", &state.showCharacter);
            ImGui::Spacing();
            ImGui::Checkbox("Show Skeleton", &state.showSkeleton);
            ImGui::Spacing();
            ImGui::Checkbox("Show Joint Names", &state.showJointNames);
            ImGui::Spacing();
            ImGui::Checkbox("Show Bone Names", &state.showBoneNames);
            ImGui::Spacing();
            ImGui::Checkbox("Wireframe", &state.showWireframe);

            ImGui::PopStyleColor();
        }
        endCard();

        // -------------------------------------------------------------
        // MODALS: Change Character / Import GLB / Edit Mapping
        // -------------------------------------------------------------
        if (showChangeCharModal) {
            ImGui::OpenPopup("Select Character##Modal");
        }
        if (ImGui::BeginPopupModal("Select Character##Modal", &showChangeCharModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(FUIStyle::accent, "%s Select Character", icons::kUser);
            ImGui::Separator();
            ImGui::Spacing();

            for (const auto& entry : chars.GetEntries()) {
                bool isSelected = (entry.id == chars.GetActiveId());
                if (ImGui::Selectable(entry.name.c_str(), isSelected, 0, ImVec2(280, 28))) {
                    chars.SelectCharacter(entry.id);
                    state.activeCharacterId = entry.id;
                    viewport.SetCharacterAsset(chars.GetActiveAsset());
                    showChangeCharModal = false;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(80, 28))) {
                showChangeCharModal = false;
            }
            ImGui::EndPopup();
        }

        if (showImportModal) {
            ImGui::OpenPopup("Import GLB Character##Modal");
        }
        if (ImGui::BeginPopupModal("Import GLB Character##Modal", &showImportModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(FUIStyle::accent, "%s Import 3D Character (GLB/glTF)", icons::kFolder);
            ImGui::Separator();
            ImGui::Spacing();

            static char importPathBuf[512] = "";
            ImGui::Text("File Path:");
            ImGui::SetNextItemWidth(360);
            ImGui::InputTextWithHint("##ImportModalPath", "e.g. assets/characters/model.glb",
                                     importPathBuf, sizeof(importPathBuf));
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_FOLDER " Browse...", ImVec2(110, 0))) {
                std::string startDir = FAppPaths::defaultCharactersDir().string();
                std::string picked;
                if (FFileDialog::openFile("glb,gltf", startDir.c_str(), picked)) {
                    strncpy_s(importPathBuf, sizeof(importPathBuf), picked.c_str(), _TRUNCATE);
                }
            }

            ImGui::Spacing();
            if (ImGui::Button("Import", ImVec2(100, 28))) {
                std::string err;
                if (chars.ImportCharacter(importPathBuf, err)) {
                    viewport.SetCharacterAsset(chars.GetActiveAsset());
                    showImportModal = false;
                    importPathBuf[0] = '\0';
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 28))) {
                showImportModal = false;
            }
            ImGui::EndPopup();
        }

        if (showMappingModal) {
            ImGui::OpenPopup("Edit Skeleton Mapping##Modal");
        }
        if (ImGui::BeginPopupModal("Edit Skeleton Mapping##Modal", &showMappingModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(FUIStyle::accent, "%s Skeleton Bone Mapping", icons::kRetarget);
            ImGui::TextDisabled("Map character rig bones to SOMA humanoid joints");
            ImGui::Separator();
            ImGui::Spacing();

            if (active && active->IsLoaded() && hasEntry) {
                if (ImGui::BeginTable("##ModalBoneMappingTable", 3,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                                      ImVec2(460, 320))) {
                    ImGui::TableSetupColumn("Character Bone", ImGuiTableColumnFlags_WidthStretch, 0.45f);
                    ImGui::TableSetupColumn("Target Joint", ImGuiTableColumnFlags_WidthStretch, 0.45f);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableHeadersRow();

                    const auto& bones = active->GetBones();
                    for (size_t b = 0; b < bones.size(); ++b) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", bones[b].name.c_str());

                        ImGui::TableSetColumnIndex(1);
                        auto it = curEntry.mapping.find(bones[b].name);
                        std::string mappedJoint = (it != curEntry.mapping.end()) ? it->second : "(none)";
                        ImGui::Text("%s", mappedJoint.c_str());

                        ImGui::TableSetColumnIndex(2);
                        if (mappedJoint != "(none)" && !mappedJoint.empty()) {
                            ImGui::TextColored(FUIStyle::green, "%s", icons::kCheck);
                        } else {
                            ImGui::TextDisabled("-");
                        }
                    }
                    ImGui::EndTable();
                }
            } else {
                ImGui::TextDisabled("Standard SOMA Humanoid: 30 / 30 joints auto-mapped 1:1.");
            }

            ImGui::Spacing();
            if (ImGui::Button("Done", ImVec2(100, 28))) {
                showMappingModal = false;
            }
            ImGui::EndPopup();
        }
    }

    // =========================================================================
    // 3. TAB 0: ANIMATION
    // =========================================================================
    else if (state.rightPanelTab == 0) {
        beginCard("##CardAnimationDetails");
        {
            drawCardHeader("Active Animation");

            std::string title = "A person eating an apple";
            if (player && player->HasAnimation() && !player->GetAnimation().skeletonName.empty()) {
                title = "Active Motion (" + player->GetAnimation().skeletonName + ")";
            }
            ImGui::TextColored(FUIStyle::text, "%s", title.c_str());
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

            if (player) {
                ImGui::TextDisabled("Current Frame: %03d / %03d", player->Frame(), fr);
                ImGui::TextDisabled("Current Time:  %.2f / %.2f s", player->GetTime(), dur);

                ImGui::Spacing();
                if (ImGui::Button(player->IsPlaying() ? "Pause Animation" : "Play Animation",
                                  ImVec2(ImGui::GetContentRegionAvail().x, 30.0f))) {
                    player->togglePlay();
                }
            }
        }
        endCard();
    }

    // =========================================================================
    // 4. TAB 2: INSPECTOR (Mesh & Rig Validation)
    // =========================================================================
    else if (state.rightPanelTab == 2) {
        FCharacterAsset* active = chars.GetActiveAsset();
        beginCard("##CardMeshInspector");
        {
            drawCardHeader("Mesh & Rig Validation");

            auto drawCheck = [](const char* label, bool ok, const std::string& desc = "") {
                if (ok) {
                    ImGui::TextColored(FUIStyle::green, "%s %s", icons::kCheck, label);
                } else {
                    ImGui::TextColored(FUIStyle::red, "%s %s", icons::kClose, label);
                }
                if (!desc.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", desc.c_str());
                }
            };

            if (active && active->IsLoaded()) {
                const auto& rep = active->GetValidationReport();
                drawCheck("Geometry", rep.hasMesh, std::to_string(rep.vertexCount) + " verts");
                drawCheck("Skeleton", rep.hasSkeleton, std::to_string(rep.boneCount) + " bones");
                drawCheck("Skinning", rep.hasSkin, rep.hasSkin ? "Skin matrices valid" : "No skin");
                drawCheck("Weights", rep.validWeights, rep.validWeights ? "Normalized" : "Check weights");
                
                char hBuf[32];
                std::snprintf(hBuf, sizeof(hBuf), "Height: %.2fm", rep.height);
                drawCheck("Rest Pose", rep.validRestPose, hBuf);

                if (!rep.warnings.empty()) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Warnings: %zu", rep.warnings.size());
                }
            } else {
                ImGui::TextDisabled("No active character asset loaded.");
            }
        }
        endCard();
    }
}

} // namespace studio
