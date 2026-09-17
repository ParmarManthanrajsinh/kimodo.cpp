#include "ui/components/HeaderBar.h"
#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "models/ModelManager.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {

void HeaderBar::draw(AppState& state, KimodoEngine& engine, ModelManager& models) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, UIStyle::topH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIStyle::bg);

    if (ImGui::Begin("##HeaderBar", nullptr, flags)) {
        // App Title & Brand
        ImGui::TextColored(UIStyle::accent, "%s", icons::kKimodo);
        ImGui::SameLine(0, 8);
        ImGui::TextColored(UIStyle::text, "KIMODO STUDIO");
        ImGui::SameLine(0, 10);
        ImGui::TextDisabled("v" KIMODO_STUDIO_VERSION);

        ImGui::SameLine(0, 32);
        // Active model badge (clickable to open models page)
        ModelEntry active;
        if (models.findCopy(models.activeId(), active)) {
            std::string btnLabel = std::string(icons::kCube) + " " + active.name + (active.installed ? " (Ready)" : " (Missing)");
            if (active.installed) {
                ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::green);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::yellow);
            }
            if (ImGui::SmallButton(btnLabel.c_str())) {
                state.screen = Screen::Models;
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::yellow);
            if (ImGui::SmallButton(ICON_FA_CUBE " No Active Model (Click to Setup)")) {
                state.screen = Screen::Models;
            }
            ImGui::PopStyleColor();
        }

        // Engine Status pill
        ImGui::SameLine(0, 24);
        EngineStatus est = engine.status();
        if (est == EngineStatus::Generating) {
            ImGui::TextColored(UIStyle::yellow, "%s Generating motion (%.0f%%)...",
                               icons::kSpinner, engine.progress() * 100.0f);
        } else if (est == EngineStatus::Finished) {
            ImGui::TextColored(UIStyle::green, "%s Generation Finished", icons::kCheck);
        } else if (est == EngineStatus::LoadingModel) {
            ImGui::TextColored(UIStyle::yellow, "%s Loading weights...", icons::kSpinner);
        } else if (est == EngineStatus::Error) {
            ImGui::TextColored(UIStyle::red, "%s Engine Error", icons::kWarn);
        } else {
            ImGui::TextDisabled("%s Idle", icons::kCheck);
        }

        // Right side stats & settings gear
        float rightWidth = 220.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - rightWidth);

        ImGui::TextDisabled("%s %s", icons::kGpu, state.gpuName.c_str());
        ImGui::SameLine(0, 16);
        ImGui::TextDisabled("%d FPS", state.fps);
        ImGui::SameLine(0, 16);

        if (ImGui::Button(icons::kSettings)) {
            state.screen = Screen::Settings;
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace studio
