#include "ui/components/NavRail.h"
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {
namespace {

bool drawNavButton(const char* icon, const char* label, bool active) {
    ImVec2 size(ImGui::GetContentRegionAvail().x, 36.0f);
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.85f, 0.44f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.14f, 0.18f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::text);
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

void NavRail::draw(AppState& state) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float sideW = state.sideWidth;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + UIStyle::topH));
    ImGui::SetNextWindowSize(ImVec2(sideW, vp->Size.y - UIStyle::topH - UIStyle::statusH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 12));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIStyle::panel);

    if (ImGui::Begin("##NavRail", nullptr, flags)) {
        ImGui::TextDisabled("WORKSPACE");
        ImGui::Spacing();

        if (drawNavButton(icons::kHome, "Home", state.screen == Screen::Home)) {
            state.screen = Screen::Home;
        }
        if (drawNavButton(icons::kGenerate, "Generate", state.screen == Screen::Generate)) {
            state.screen = Screen::Generate;
            state.lastToolScreen = Screen::Generate;
        }
        if (drawNavButton(icons::kRetarget, "Retarget", state.screen == Screen::Retarget)) {
            state.screen = Screen::Retarget;
            state.lastToolScreen = Screen::Retarget;
        }
        if (drawNavButton(icons::kExport, "Export", state.screen == Screen::Export)) {
            state.screen = Screen::Export;
            state.lastToolScreen = Screen::Export;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("ASSETS");
        ImGui::Spacing();

        if (drawNavButton(icons::kFolder, "Library", state.screen == Screen::Library)) {
            state.screen = Screen::Library;
        }
        if (drawNavButton(icons::kUser, "Characters", state.screen == Screen::Characters)) {
            state.screen = Screen::Characters;
        }
        if (drawNavButton(icons::kCube, "Models", state.screen == Screen::Models)) {
            state.screen = Screen::Models;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("SYSTEM");
        ImGui::Spacing();

        if (drawNavButton(icons::kSettings, "Settings", state.screen == Screen::Settings)) {
            state.screen = Screen::Settings;
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace studio
