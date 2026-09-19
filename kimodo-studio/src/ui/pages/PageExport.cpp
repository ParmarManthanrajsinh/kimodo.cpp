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
#include "utils/FileDialog.h"

#include <filesystem>

namespace studio {

void PageExport::Draw(AppState& state, AnimationPlayer& player, AnimationLibrary& library, CharacterLibrary& chars,
                      Toasts& toasts) {
    (void)state;
    (void)library;
    ImGui::TextColored(UIStyle::accent, "%s Motion Export", icons::kExport);
    ImGui::TextDisabled("Export animation for Unreal Engine, Blender, Maya, and game engines");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const Animation& current_anim = player.GetAnimation();
    if (current_anim.empty()) {
        ImGui::TextDisabled("No active animation loaded in viewport. Generate or load an animation first.");
        return;
    }

    // 1. Export Format / Mode
    static int export_format = 0; // 0 = BVH Humanoid, 1 = Skeleton GLB, 2 = Full Character GLB
    ImGui::Text("Export Format & Pipeline:");
    ImGui::RadioButton("BVH Humanoid (Recommended for Unreal / Maya)", &export_format, 0);
    ImGui::RadioButton("Skeleton Animation GLB (Lightweight bones for Blender / Web)", &export_format, 1);
    ImGui::RadioButton("Full Character GLB (Mesh + Skin + Materials + Motion)", &export_format, 2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. Export Destination & Filename
    auto& settings = SettingsManager::GetInstance().GetSettings();
    static char export_dir_buf[512] = "";
    if (export_dir_buf[0] == '\0') {
        std::string d = settings.export_dir.empty() ? AppPaths::DefaultExportDir().string() : settings.export_dir;
        strncpy_s(export_dir_buf, sizeof(export_dir_buf), d.c_str(), sizeof(export_dir_buf) - 1);
    }
    ImGui::Text("Export Directory:");
    ImGui::SetNextItemWidth(-84);
    if (ImGui::InputText("##ExportDir", export_dir_buf, sizeof(export_dir_buf))) {
        settings.export_dir = export_dir_buf;
        SettingsManager::GetInstance().save();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER " Browse...", ImVec2(76, 0))) {
        std::string picked;
        if (FileDialog::PickFolder(export_dir_buf[0] != '\0' ? export_dir_buf : nullptr, picked)) {
            strncpy_s(export_dir_buf, sizeof(export_dir_buf), picked.c_str(), _TRUNCATE);
            settings.export_dir = export_dir_buf;
            SettingsManager::GetInstance().save();
        } else if (FileDialog::GetLastError() && FileDialog::GetLastError()[0] != '\0') {
            toasts.Push(std::string("Folder picker failed: ") + FileDialog::GetLastError(), ToastKind::Error);
        }
    }

    static char filename_buf[256] = "kimodo_motion";
    ImGui::Text("Filename (without extension):");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##ExportFilename", filename_buf, sizeof(filename_buf));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3. Export Parameters
    ImGui::Text("Parameters:");
    static float export_fps = 30.0f;
    ImGui::SliderFloat("FPS", &export_fps, 15.0f, 120.0f, "%.0f FPS");

    static int root_motion_mode = 0; // 0 = Preserve, 1 = LockX, 2 = LockXZ, 3 = Zero
    const char* rm_options[] = {"Preserve Root Motion", "Lock X Translation", "Lock XZ (In-Place)",
                                "Zero Root Translation"};
    ImGui::Combo("Root Motion", &root_motion_mode, rm_options, 4);

    static bool use_frame_range = false;
    static int start_frame = 0;
    static int end_frame = current_anim.frames - 1;
    if (end_frame >= current_anim.frames)
        end_frame = current_anim.frames - 1;

    ImGui::Checkbox("Limit Frame Range", &use_frame_range);
    if (use_frame_range) {
        ImGui::SliderInt("Start Frame", &start_frame, 0, current_anim.frames - 1);
        ImGui::SliderInt("End Frame", &end_frame, start_frame, current_anim.frames - 1);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 4. Quick Unreal Workflow Card
    if (export_format == 0) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.20f, 0.9f));
        ImGui::BeginChild("##UeInfo", ImVec2(0, 75), true);
        {
            ImGui::TextColored(UIStyle::accent, "%s Unreal Engine Export Ready:", icons::kInfo);
            ImGui::Text("Produces standard humanoid BVH with valid root hierarchy and Euler angles.");
            ImGui::TextDisabled(
                "In UE5: Import BVH -> Create IK Rig -> Use IK Retargeter -> Retarget to Manny / MetaHuman.");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // 5. Export Action Button
    ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));

    std::string btn_label = (export_format == 0)   ? (ICON_FA_EXPORT "  Export BVH Humanoid")
                            : (export_format == 1) ? (ICON_FA_EXPORT "  Export Skeleton GLB")
                                                   : (ICON_FA_EXPORT "  Export Full Character GLB");

    if (ImGui::Button(btn_label.c_str(), ImVec2(-1, 44))) {
        std::string ext = (export_format == 0) ? ".bvh" : ".glb";

        // Native Save As dialog starting in the current export directory.
        // User confirms/edits the exact output file (filter enforces the right extension).
        const std::string filter = ext.substr(1); // NFD filter format: extension without dot, e.g. "bvh"
        std::string save_path;
        if (!FileDialog::SaveFile(filter.c_str(), export_dir_buf[0] != '\0' ? export_dir_buf : nullptr, save_path)) {
            if (FileDialog::GetLastError() && FileDialog::GetLastError()[0] != '\0') {
                toasts.Push(std::string("Save dialog failed: ") + FileDialog::GetLastError(), ToastKind::Error);
            }
            return; // user cancelled or dialog failed - keep previous state
        }

        // Reflect the user's dialog choice back into the directory field for next time.
        std::filesystem::path chosen(save_path);
        strncpy_s(export_dir_buf, sizeof(export_dir_buf), chosen.parent_path().string().c_str(), _TRUNCATE);
        settings.export_dir = export_dir_buf;
        SettingsManager::GetInstance().save();
        strncpy_s(filename_buf, sizeof(filename_buf), chosen.filename().replace_extension().string().c_str(),
                  _TRUNCATE);

        std::filesystem::path out_path = save_path;

        ExportOptions opts;
        opts.path = out_path.string();
        opts.fps = export_fps;
        opts.root_motion = (root_motion_mode == 1)   ? RootMotion::LockX
                           : (root_motion_mode == 2) ? RootMotion::LockXZ
                           : (root_motion_mode == 3) ? RootMotion::Zero
                                                     : RootMotion::Preserve;

        std::string err;
        std::string rep;
        bool ok = false;

        if (export_format == 0) {
            // BVH Exporter
            ok = BVHExporter::ExportWithRange(current_anim, opts, use_frame_range ? start_frame : -1,
                                              use_frame_range ? end_frame : -1, err, &rep);
        } else if (export_format == 1) {
            // Skeleton GLB Exporter
            GLBExporter glb_exp;
            ok = glb_exp.ExportAnimation(current_anim, opts, err);
            rep = glb_exp.GetLastReport();
        } else {
            // Full Character GLB Exporter
            const CharacterAsset* char_asset = chars.GetActiveAsset();
            if (char_asset && char_asset->IsLoaded()) {
                CharacterEntry cur_entry;
                chars.FindEntry(chars.GetActiveId(), cur_entry);
                ok = CharacterGLBExporter::ExportCharacterGLB(*char_asset, current_anim, cur_entry.mapping, opts, err,
                                                              &rep);
            } else {
                err = "No active character loaded for full mesh export. Select a character first.";
            }
        }

        if (ok) {
            toasts.Push("Export successful: " + out_path.filename().string(), ToastKind::Success);
        } else {
            toasts.Push("Export failed: " + err, ToastKind::Error);
        }
    }
    ImGui::PopStyleColor(2);
}

} // namespace studio
