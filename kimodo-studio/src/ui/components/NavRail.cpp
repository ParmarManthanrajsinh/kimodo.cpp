#include "ui/components/NavRail.h"
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {
namespace {

bool drawNavButton(const char* icon, const char* label, bool active) {
    ImVec2 size(UIStyle::sideW - 16, 38);
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::text);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.15f, 0.5f));

    std::string text = std::string(icon) + "  " + label;
    bool clicked = ImGui::Button(text.c_str(), size);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    return clicked;
}

} // namespace

void NavRail::draw(AppState& state) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + UIStyle::topH));
    ImGui::SetNextWindowSize(ImVec2(UIStyle::sideW, vp->Size.y - UIStyle::topH - UIStyle::statusH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 12));
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
