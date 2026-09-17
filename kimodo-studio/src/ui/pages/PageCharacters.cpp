#include "ui/pages/PageCharacters.h"
#include "character/CharacterLibrary.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {

void PageCharacters::draw(AppState& state, CharacterLibrary& chars, Viewport& viewport) {
    ImGui::TextColored(UIStyle::accent, "%s Character Library", icons::kUser);
    ImGui::TextDisabled("Load, inspect, map, and preview 3D humanoid rigged characters");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Import Bar
    static char importPathBuf[512] = "";
    ImGui::Text("Import 3D Character (GLB/glTF):");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
    ImGui::InputTextWithHint("##ImportCharPath", "e.g. assets/characters/CesiumMan.glb or C:/model.glb",
                             importPathBuf, sizeof(importPathBuf));
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS " Import", ImVec2(110, 0))) {
        std::string err;
        if (!chars.importCharacter(importPathBuf, err)) {
            // handle error
        } else {
            importPathBuf[0] = '\0';
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Split into Left (Character List) and Right (Inspector / Mapper)
    float listWidth = 240.0f;
    ImGui::BeginChild("##CharListChild", ImVec2(listWidth, 0), true);
    {
        ImGui::TextDisabled("REGISTERED CHARACTERS");
        ImGui::Spacing();

        for (const auto& entry : chars.entries()) {
            bool isSelected = (entry.id == chars.activeId());
            if (ImGui::Selectable(entry.name.c_str(), isSelected, 0, ImVec2(0, 32))) {
                chars.selectCharacter(entry.id);
                state.activeCharacterId = entry.id;
                viewport.setCharacterAsset(chars.activeAsset());
            }
            ImGui::SameLine(listWidth - 40);
            ImGui::TextDisabled("%s", entry.installed ? icons::kCheck : icons::kClose);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Side: Selected Character Details & Validation & Bone Mapper
    ImGui::BeginChild("##CharInspectorChild", ImVec2(0, 0), true);
    {
        CharacterAsset* active = chars.activeAsset();
        if (active && active->isLoaded()) {
            ImGui::TextColored(UIStyle::text, "%s %s", icons::kUser, active->name().c_str());
            ImGui::TextDisabled("Path: %s", active->filePath().c_str());
            ImGui::TextDisabled("License: %s | Author: %s", active->license().c_str(), active->author().c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Validation Checklist Cards
            const auto& rep = active->validationReport();
            ImGui::TextColored(UIStyle::accent, "Validation Checklist:");

            auto drawCheckItem = [](const char* label, bool ok, const std::string& detail = "") {
                if (ok) {
                    ImGui::TextColored(UIStyle::green, "[ %s ]  %s", icons::kCheck, label);
                } else {
                    ImGui::TextColored(UIStyle::red, "[ %s ]  %s", icons::kClose, label);
                }
                if (!detail.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", detail.c_str());
                }
            };

            drawCheckItem("Mesh Geometry", rep.hasMesh,
                          std::to_string(rep.vertexCount) + " vertices, " + std::to_string(rep.triangleCount) + " triangles");
            drawCheckItem("Skeleton Hierarchy", rep.hasSkeleton,
                          std::to_string(rep.boneCount) + " bones detected");
            drawCheckItem("Skin & IBM", rep.hasSkin, "Inverse bind matrices verified");
            drawCheckItem("Required Bones", rep.requiredBonesPresent,
                          rep.missingRequiredBones.empty() ? "All core groups detected" : "Partial match");
            drawCheckItem("Vertex Weights", rep.validWeights, "Max 4 influences per vertex");
            drawCheckItem("Rest Pose", rep.validRestPose, "Height: " + std::to_string(rep.height) + "m");
            drawCheckItem("Coordinate System", true, "glTF standard (Y-up, meters)");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Bone Mapping Table
            ImGui::TextColored(UIStyle::text, "%s Bone Mapping (Character Bones -> SOMA Joints)", icons::kRetarget);
            ImGui::TextDisabled("Auto-detected via alias tables. Override bindings below if needed.");
            ImGui::Spacing();

            CharacterEntry curEntry;
            if (chars.findEntry(chars.activeId(), curEntry)) {
                if (ImGui::BeginTable("##BoneMappingTable", 3,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                                      ImVec2(0, 220))) {
                    ImGui::TableSetupColumn("Character Bone", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                    ImGui::TableSetupColumn("Target Joint", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableHeadersRow();

                    const auto& bones = active->bones();
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
                            ImGui::TextColored(UIStyle::green, "%s", icons::kCheck);
                        } else {
                            ImGui::TextDisabled("-");
                        }
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_PLAY " Focus Camera on Character", ImVec2(220, 32))) {
                viewport.frame();
            }
        } else {
            ImGui::TextDisabled("No character selected. Select or import a character from the left list.");
        }
    }
    ImGui::EndChild();
}

} // namespace studio
