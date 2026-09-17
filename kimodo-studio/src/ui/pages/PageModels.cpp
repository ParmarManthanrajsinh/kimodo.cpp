#include "ui/pages/PageModels.h"
#include "imgui.h"
#include "models/ModelManager.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"

#include <vector>

namespace studio {

void PageModels::draw(AppState& state, ModelManager& models, Toasts& toasts) {
    (void)state;
    ImGui::TextColored(UIStyle::accent, "%s Model Registry & Weights", icons::kCube);
    ImGui::TextDisabled("Manage diffusion weights (GGUF) and text encoder bundles");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Top action bar
    if (ImGui::Button(ICON_FA_REPEAT " Rescan Model Directories")) {
        models.rescan();
        toasts.push("Rescanned model directories", ToastKind::Info);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Models directory: %s", models.modelDir().c_str());

    ImGui::Spacing();

    // Active Task progress bar
    if (models.busy()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.22f, 0.9f));
        ImGui::BeginChild("##ModelTaskBox", ImVec2(0, 75), true);
        {
            ImGui::TextColored(UIStyle::yellow, "%s %s", icons::kSpinner, models.taskLabel().c_str());
            ImGui::ProgressBar(models.taskProgress(), ImVec2(-100, 24));
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_CLOSE " Cancel", ImVec2(90, 24))) {
                models.cancelTask();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();

    const auto& entries = models.entries();
    if (entries.empty()) {
        ImGui::TextDisabled("No model entries found in configuration.");
        return;
    }

    static char importPathBuf[512] = "";

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

            ImGui::TextDisabled("ID: %s | Skeleton: %s | License: %s",
                                e.id.c_str(), e.skeleton.c_str(), e.license.c_str());

            if (e.installed) {
                float sizeMb = static_cast<float>(e.localBytes) / (1024.0f * 1024.0f);
                ImGui::TextDisabled("File: %s (%.1f MB)", e.localPath.c_str(), sizeMb);

                ImGui::Spacing();
                bool isActive = (e.id == models.activeId());
                if (isActive) {
                    ImGui::TextColored(UIStyle::green, "%s Active Generation Model", icons::kCheck);
                } else {
                    if (ImGui::Button("Set as Active Model")) {
                        models.select(e.id);
                        toasts.push("Active model set to: " + e.name, ToastKind::Success);
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_CHECK " Verify Checksum")) {
                    models.verifyAsync(e.id);
                    toasts.push("Verifying SHA-256 for " + e.name, ToastKind::Info);
                }

                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_TRASH " Delete")) {
                    models.deleteAsync(e.id);
                    toasts.push("Deleted model: " + e.name, ToastKind::Info);
                }
            } else {
                float expectedMb = static_cast<float>(e.sizeBytes) / (1024.0f * 1024.0f);
                ImGui::TextDisabled("Expected File: %s (~%.1f MB)", e.motionFile.c_str(), expectedMb);
                ImGui::TextDisabled("Hugging Face: %s", e.repo.c_str());

                ImGui::Spacing();
                if (ImGui::Button(ICON_FA_DOWNLOAD " Download from Hugging Face")) {
                    models.downloadAsync(e.id);
                    toasts.push("Download started for: " + e.name, ToastKind::Info);
                }

                ImGui::Spacing();
                ImGui::Text("Or import local .gguf file:");
                ImGui::SetNextItemWidth(-120);
                ImGui::InputText("##ImportPath", importPathBuf, sizeof(importPathBuf));
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_FOLDER " Import")) {
                    if (importPathBuf[0] != '\0') {
                        models.importAsync(importPathBuf, e.id);
                        toasts.push("Importing model from local file...", ToastKind::Info);
                    } else {
                        toasts.push("Please enter a valid file path", ToastKind::Warning);
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
