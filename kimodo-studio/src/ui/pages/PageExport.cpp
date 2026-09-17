#include "ui/pages/PageExport.h"
#include "animation/AnimationPlayer.h"
#include "app/SettingsManager.h"
#include "character/CharacterLibrary.h"
#include "export/BVHExporter.h"
#include "export/CharacterGLBExporter.h"
#include "export/GLBExporter.h"
#include "imgui.h"
#include "library/AnimationLibrary.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "utils/AppPaths.h"

#include <filesystem>

namespace studio {

void PageExport::draw(AppState& state, AnimationPlayer& player,
                      AnimationLibrary& library, CharacterLibrary& chars,
                      Toasts& toasts) {
    (void)state;
    (void)library;
    ImGui::TextColored(UIStyle::accent, "%s Motion Export", icons::kExport);
    ImGui::TextDisabled("Export animation for Unreal Engine, Blender, Maya, and game engines");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const Animation& currentAnim = player.animation();
    if (currentAnim.empty()) {
        ImGui::TextDisabled("No active animation loaded in viewport. Generate or load an animation first.");
        return;
    }

    // 1. Export Format / Mode
    static int exportFormat = 0; // 0 = BVH Humanoid, 1 = Skeleton GLB, 2 = Full Character GLB
    ImGui::Text("Export Format & Pipeline:");
    ImGui::RadioButton("BVH Humanoid (Recommended for Unreal / Maya)", &exportFormat, 0);
    ImGui::RadioButton("Skeleton Animation GLB (Lightweight bones for Blender / Web)", &exportFormat, 1);
    ImGui::RadioButton("Full Character GLB (Mesh + Skin + Materials + Motion)", &exportFormat, 2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. Export Destination & Filename
    auto& settings = SettingsManager::instance().settings();
    static char exportDirBuf[512] = "";
    if (exportDirBuf[0] == '\0') {
        std::string d = settings.exportDir.empty() ? AppPaths::defaultExportDir().string() : settings.exportDir;
        strncpy_s(exportDirBuf, sizeof(exportDirBuf), d.c_str(), sizeof(exportDirBuf) - 1);
    }
    ImGui::Text("Export Directory:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##ExportDir", exportDirBuf, sizeof(exportDirBuf))) {
        settings.exportDir = exportDirBuf;
        SettingsManager::instance().save();
    }

    static char filenameBuf[256] = "kimodo_motion";
    ImGui::Text("Filename (without extension):");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ExportFilename", filenameBuf, sizeof(filenameBuf));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3. Export Parameters
    ImGui::Text("Parameters:");
    static float exportFps = 30.0f;
    ImGui::SliderFloat("FPS", &exportFps, 15.0f, 120.0f, "%.0f FPS");

    static int rootMotionMode = 0; // 0 = Preserve, 1 = LockX, 2 = LockXZ, 3 = Zero
    const char* rmOptions[] = {"Preserve Root Motion", "Lock X Translation", "Lock XZ (In-Place)", "Zero Root Translation"};
    ImGui::Combo("Root Motion", &rootMotionMode, rmOptions, 4);

    static bool useFrameRange = false;
    static int startFrame = 0;
    static int endFrame = currentAnim.frames - 1;
    if (endFrame >= currentAnim.frames) endFrame = currentAnim.frames - 1;

    ImGui::Checkbox("Limit Frame Range", &useFrameRange);
    if (useFrameRange) {
        ImGui::SliderInt("Start Frame", &startFrame, 0, currentAnim.frames - 1);
        ImGui::SliderInt("End Frame", &endFrame, startFrame, currentAnim.frames - 1);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 4. Quick Unreal Workflow Card
    if (exportFormat == 0) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.20f, 0.9f));
        ImGui::BeginChild("##UeInfo", ImVec2(0, 75), true);
        {
            ImGui::TextColored(UIStyle::accent, "%s Unreal Engine Export Ready:", icons::kInfo);
            ImGui::Text("Produces standard humanoid BVH with valid root hierarchy and Euler angles.");
            ImGui::TextDisabled("In UE5: Import BVH -> Create IK Rig -> Use IK Retargeter -> Retarget to Manny / MetaHuman.");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // 5. Export Action Button
    ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));

    std::string btnLabel = (exportFormat == 0) ? (ICON_FA_EXPORT "  Export BVH Humanoid") :
                           (exportFormat == 1) ? (ICON_FA_EXPORT "  Export Skeleton GLB") :
                                                 (ICON_FA_EXPORT "  Export Full Character GLB");

    if (ImGui::Button(btnLabel.c_str(), ImVec2(-1, 44))) {
        std::string ext = (exportFormat == 0) ? ".bvh" : ".glb";
        std::filesystem::path outPath = std::filesystem::path(exportDirBuf) / (std::string(filenameBuf) + ext);

        ExportOptions opts;
        opts.path = outPath.string();
        opts.fps = exportFps;
        opts.rootMotion = (rootMotionMode == 1) ? RootMotion::LockX :
                          (rootMotionMode == 2) ? RootMotion::LockXZ :
                          (rootMotionMode == 3) ? RootMotion::Zero : RootMotion::Preserve;

        std::string err;
        std::string rep;
        bool ok = false;

        if (exportFormat == 0) {
            // BVH Exporter
            ok = BVHExporter::exportWithRange(currentAnim, opts,
                                             useFrameRange ? startFrame : -1,
                                             useFrameRange ? endFrame : -1,
                                             err, &rep);
        } else if (exportFormat == 1) {
            // Skeleton GLB Exporter
            GLBExporter glbExp;
            ok = glbExp.exportAnimation(currentAnim, opts, err);
            rep = glbExp.lastReport();
        } else {
            // Full Character GLB Exporter
            const CharacterAsset* charAsset = chars.activeAsset();
            if (charAsset && charAsset->isLoaded()) {
                CharacterEntry curEntry;
                chars.findEntry(chars.activeId(), curEntry);
                ok = CharacterGLBExporter::exportCharacterGLB(*charAsset, currentAnim,
                                                             curEntry.mapping, opts, err, &rep);
            } else {
                err = "No active character loaded for full mesh export. Select a character first.";
            }
        }

        if (ok) {
            toasts.push("Export successful: " + outPath.filename().string(), ToastKind::Success);
        } else {
            toasts.push("Export failed: " + err, ToastKind::Error);
        }
    }
    ImGui::PopStyleColor(2);
}

} // namespace studio
