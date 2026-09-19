#include "ui/pages/PageModels.h"
#include "imgui.h"
#include "models/ModelManager.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "utils/AppPaths.h"
#include "utils/FileDialog.h"

#include <vector>

namespace studio {

void PageModels::Draw(AppState& state, ModelManager& models, Toasts& toasts) {
    (void)state;
    ImGui::TextColored(UIStyle::accent, "%s Model Registry & Weights", icons::kCube);
    ImGui::TextDisabled("Manage diffusion weights (GGUF) and text encoder bundles");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Top action bar
    if (ImGui::Button(ICON_FA_REPEAT " Rescan Model Directories")) {
        models.Rescan();
        toasts.Push("Rescanned model directories", ToastKind::Info);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Models directory: %s", models.GetModelDir().c_str());

    ImGui::Spacing();

    // Active Task progress bar
    if (models.IsBusy()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.22f, 0.9f));
        ImGui::BeginChild("##ModelTaskBox", ImVec2(0, 75), true);
        {
            ImGui::TextColored(UIStyle::yellow, "%s %s", icons::kSpinner, models.GetTaskLabel().c_str());
            ImGui::ProgressBar(models.GetTaskProgress(), ImVec2(-100, 24));
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_CLOSE " Cancel", ImVec2(90, 24))) {
                models.CancelTask();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();

    const auto& entries = models.GetEntries();
    if (entries.empty()) {
        ImGui::TextDisabled("No model entries found in configuration.");
        return;
    }

    static char import_path_buf[512] = "";

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        ImGui::PushID(static_cast<int>(i));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.11f, 0.14f, 0.8f));
        ImGui::BeginChild("##ModelCard", ImVec2(0, e.installed ? 150.0f : 200.0f), true);
        {
            ImGui::TextColored(UIStyle::text, "%s %s", icons::kCube, e.name.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 140);
            if (e.installed) {
                ImGui::TextColored(UIStyle::green, "%s INSTALLED", icons::kCheck);
            } else {
                ImGui::TextColored(UIStyle::yellow, "%s MISSING", icons::kWarn);
            }

            ImGui::TextDisabled("ID: %s | Skeleton: %s | License: %s", e.id.c_str(), e.skeleton.c_str(),
                                e.license.c_str());

            if (e.installed) {
                float size_mb = static_cast<float>(e.local_bytes) / (1024.0f * 1024.0f);
                ImGui::TextDisabled("File: %s (%.1f MB)", e.local_path.c_str(), size_mb);

                ImGui::Spacing();
                bool is_active = (e.id == models.GetActiveId());
                if (is_active) {
                    ImGui::TextColored(UIStyle::green, "%s Active Generation Model", icons::kCheck);
                } else {
                    if (ImGui::Button("Set as Active Model")) {
                        models.Select(e.id);
                        toasts.Push("Active model set to: " + e.name, ToastKind::Success);
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_CHECK " Verify Checksum")) {
                    models.VerifyAsync(e.id);
                    toasts.Push("Verifying SHA-256 for " + e.name, ToastKind::Info);
                }

                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_TRASH " Delete")) {
                    models.DeleteAsync(e.id);
                    toasts.Push("Deleted model: " + e.name, ToastKind::Info);
                }
            } else {
                float expected_mb = static_cast<float>(e.size_bytes) / (1024.0f * 1024.0f);
                ImGui::TextDisabled("Expected File: %s (~%.1f MB)", e.motion_file.c_str(), expected_mb);
                ImGui::TextDisabled("Hugging Face: %s", e.repo.c_str());

                ImGui::Spacing();
                if (ImGui::Button(ICON_FA_DOWNLOAD " Download from Hugging Face")) {
                    models.DownloadAsync(e.id);
                    toasts.Push("Download started for: " + e.name, ToastKind::Info);
                }

                ImGui::Spacing();
                ImGui::Text("Or import local .gguf file:");
                ImGui::SetNextItemWidth(-120);
                ImGui::InputText("##ImportPath", import_path_buf, sizeof(import_path_buf));
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_FOLDER " Browse...")) {
                    std::string start_dir = AppPaths::DefaultModelsDir().string();
                    std::string picked;
                    if (FileDialog::OpenFile("gguf", start_dir.c_str(), picked)) {
                        strncpy_s(import_path_buf, sizeof(import_path_buf), picked.c_str(), _TRUNCATE);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_FOLDER " Import")) {
                    if (import_path_buf[0] != '\0') {
                        models.ImportAsync(import_path_buf, e.id);
                        toasts.Push("Importing model from local file...", ToastKind::Info);
                    } else {
                        toasts.Push("Please enter a valid file path", ToastKind::Warning);
                    }
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PopID();
    }
}

} // namespace studio
