#include "ui/components/HeaderBar.h"
#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "models/ModelManager.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UIHelpers.h"

namespace studio {

void HeaderBar::Draw(AppState& state, KimodoEngine& engine, ModelManager& models) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, UIStyle::top_h));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    const float pad_y = (UIStyle::top_h - 26.0f) * 0.5f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, pad_y));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIStyle::bg);

    if (ImGui::Begin("##HeaderBar", nullptr, flags)) {
        // App Title & Brand
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(UIStyle::accent, "%s", icons::kKimodo);
        ImGui::SameLine(0, 8);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(UIStyle::text, "KIMODO STUDIO");
        ImGui::SameLine(0, 8);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("v" KIMODO_STUDIO_VERSION);

        ImGui::SameLine(0, 24);

        // Active model badge in pill
        ModelEntry active;
        bool has_model = models.find_copy(models.GetActiveId(), active);
        std::string model_title =
            has_model ? (active.name + (active.installed ? " (Ready)" : " (Missing)")) : "SOMA RP v1.1 (Ready)";

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.22f, 0.12f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.30f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::accent);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.50f, 0.26f, 0.8f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        std::string btn_label = std::string(icons::kCube) + "  " + model_title;
        if (ImGui::Button(btn_label.c_str(), ImVec2(0, 26))) {
            state.screen = Screen::Models;
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        // Engine Status pill
        ImGui::SameLine(0, 18);
        ImGui::AlignTextToFramePadding();
        EngineStatus est = engine.GetStatus();
        if (est == EngineStatus::Generating) {
            ImGui::TextColored(UIStyle::yellow, "%s Generating motion (%.0f%%)...", icons::kSpinner,
                               engine.GetProgress() * 100.0f);
        } else if (est == EngineStatus::Finished) {
            ImGui::TextColored(UIStyle::green, "%s Generation Finished", icons::kCheck);
        } else if (est == EngineStatus::LoadingModel) {
            ImGui::TextColored(UIStyle::yellow, "%s Loading weights...", icons::kSpinner);
        } else if (est == EngineStatus::Error) {
            ImGui::TextColored(UIStyle::red, "%s Engine Error", icons::kWarn);
        } else {
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.70f, 1.0f), "%s Idle", icons::kCheck);
        }

        // Right side stats & settings gear
        const float right_width = 180.0f;
        if (ImGui::GetContentRegionAvail().x > right_width) {
            ImGui::SameLine(ImGui::GetWindowWidth() - right_width - 16.0f);
        } else {
            ImGui::SameLine(0, 16);
        }

        // FPS
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(UIStyle::text, "%d FPS", state.fps > 0 ? state.fps : 60);

        // Settings gear button
        ImGui::SameLine(0, 16);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        std::string settings_btn = std::string(icons::kSettings) + " Settings";
        if (ImGui::Button(settings_btn.c_str(), ImVec2(0, 26))) {
            state.screen = Screen::settings;
        }
        ImGui::PopStyleVar(1);
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);
}

} // namespace studio
