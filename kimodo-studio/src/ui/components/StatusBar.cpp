#include "ui/components/StatusBar.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {

void StatusBar::Draw(AppState& state, Viewport& viewport) {
    (void)viewport;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - UIStyle::status_h));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, UIStyle::status_h));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIStyle::bg);

    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        // Left side: ● Ready | Loaded animation: ...
        ImGui::TextColored(UIStyle::green, "%s", icons::kCheckCircle);
        ImGui::SameLine(0, 6);
        ImGui::TextColored(UIStyle::text, "Ready");

        ImGui::SameLine(0, 14);
        ImGui::TextDisabled("|");
        ImGui::SameLine(0, 14);

        std::string anim_name = !state.prompt.empty() ? state.prompt : "None";
        ImGui::TextDisabled("Loaded animation:");
        ImGui::SameLine(0, 6);
        ImGui::TextColored(UIStyle::text, "%s", anim_name.c_str());

        // Right side: OpenGL 3.3 | 60 FPS | Kimodo Studio 0.1.0
        float right_w = 280.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - right_w);

        ImGui::TextDisabled("OpenGL 3.3");
        ImGui::SameLine(0, 12);
        ImGui::TextDisabled("|");
        ImGui::SameLine(0, 12);
        ImGui::TextDisabled("%d FPS", state.fps > 0 ? state.fps : 60);
        ImGui::SameLine(0, 12);
        ImGui::TextDisabled("|");
        ImGui::SameLine(0, 12);
        ImGui::TextDisabled("Kimodo Studio " KIMODO_STUDIO_VERSION);
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace studio
