#include "ui/components/NavRail.h"
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {
namespace {

bool drawNavButton(const char* icon, const char* label, bool active) {
    ImVec2 size(ImGui::GetContentRegionAvail().x, 36.0f);
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, FUIStyle::accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.85f, 0.44f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.14f, 0.18f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, FUIStyle::text);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 6.0f));

    std::string text = std::string(icon) + "   " + label;
    bool clicked = ImGui::Button(text.c_str(), size);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    return clicked;
}

} // namespace

void SNavRail::Draw(FAppState& state) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float sideW = state.sideWidth;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + FUIStyle::topH));
    ImGui::SetNextWindowSize(ImVec2(sideW, vp->Size.y - FUIStyle::topH - FUIStyle::statusH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 12));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, FUIStyle::panel);

    if (ImGui::Begin("##NavRail", nullptr, flags)) {
        ImGui::TextDisabled("WORKSPACE");
        ImGui::Spacing();

        if (drawNavButton(icons::kHome, "Home", state.screen == EScreen::Home)) {
            state.screen = EScreen::Home;
        }
        if (drawNavButton(icons::kGenerate, "Generate", state.screen == EScreen::Generate)) {
            state.screen = EScreen::Generate;
            state.lastToolScreen = EScreen::Generate;
        }
        if (drawNavButton(icons::kRetarget, "Retarget", state.screen == EScreen::Retarget)) {
            state.screen = EScreen::Retarget;
            state.lastToolScreen = EScreen::Retarget;
        }
        if (drawNavButton(icons::kExport, "Export", state.screen == EScreen::Export)) {
            state.screen = EScreen::Export;
            state.lastToolScreen = EScreen::Export;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("ASSETS");
        ImGui::Spacing();

        if (drawNavButton(icons::kFolder, "Library", state.screen == EScreen::Library)) {
            state.screen = EScreen::Library;
        }
        if (drawNavButton(icons::kUser, "Characters", state.screen == EScreen::Characters)) {
            state.screen = EScreen::Characters;
        }
        if (drawNavButton(icons::kCube, "Models", state.screen == EScreen::Models)) {
            state.screen = EScreen::Models;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("SYSTEM");
        ImGui::Spacing();

        if (drawNavButton(icons::kSettings, "Settings", state.screen == EScreen::Settings)) {
            state.screen = EScreen::Settings;
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace studio
