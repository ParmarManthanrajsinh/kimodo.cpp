#include "ui/components/HeaderBar.h"
#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "models/ModelManager.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UIHelpers.h"

namespace studio {

void SHeaderBar::Draw(FAppState& state, FKimodoEngine& engine, FModelManager& models) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, FUIStyle::topH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    const float padY = (FUIStyle::topH - 26.0f) * 0.5f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, padY));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, FUIStyle::bg);

    if (ImGui::Begin("##HeaderBar", nullptr, flags)) {
        // App Title & Brand
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(FUIStyle::accent, "%s", icons::kKimodo);
        ImGui::SameLine(0, 8);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(FUIStyle::text, "KIMODO STUDIO");
        ImGui::SameLine(0, 8);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("v" KIMODO_STUDIO_VERSION);

        ImGui::SameLine(0, 24);

        // Active model badge in pill
        FModelEntry active;
        bool hasModel = models.findCopy(models.GetActiveId(), active);
        std::string modelTitle = hasModel ? (active.name + (active.installed ? " (Ready)" : " (Missing)")) : "SOMA RP v1.1 (Ready)";

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.22f, 0.12f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.30f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, FUIStyle::accent);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.50f, 0.26f, 0.8f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        std::string btnLabel = std::string(icons::kCube) + "  " + modelTitle;
        if (ImGui::Button(btnLabel.c_str(), ImVec2(0, 26))) {
            state.screen = EScreen::Models;
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        // Engine Status pill
        ImGui::SameLine(0, 18);
        ImGui::AlignTextToFramePadding();
        EEngineStatus est = engine.GetStatus();
        if (est == EEngineStatus::Generating) {
            ImGui::TextColored(FUIStyle::yellow, "%s Generating motion (%.0f%%)...",
                               icons::kSpinner, engine.GetProgress() * 100.0f);
        } else if (est == EEngineStatus::Finished) {
            ImGui::TextColored(FUIStyle::green, "%s Generation Finished", icons::kCheck);
        } else if (est == EEngineStatus::LoadingModel) {
            ImGui::TextColored(FUIStyle::yellow, "%s Loading weights...", icons::kSpinner);
        } else if (est == EEngineStatus::Error) {
            ImGui::TextColored(FUIStyle::red, "%s Engine Error", icons::kWarn);
        } else {
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f), "%s Idle", icons::kCheck);
        }

        // Right side stats & settings gear
        const float rightWidth = 180.0f;
        if (ImGui::GetContentRegionAvail().x > rightWidth) {
            ImGui::SameLine(ImGui::GetWindowWidth() - rightWidth - 16.0f);
        } else {
            ImGui::SameLine(0, 16);
        }

        // FPS
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(FUIStyle::text, "%d FPS", state.fps > 0 ? state.fps : 60);

        // Settings gear button
        ImGui::SameLine(0, 16);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        std::string settingsBtn = std::string(icons::kSettings) + " Settings";
        if (ImGui::Button(settingsBtn.c_str(), ImVec2(0, 26))) {
            state.screen = EScreen::Settings;
        }
        ImGui::PopStyleVar(1);
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);
}

} // namespace studio
