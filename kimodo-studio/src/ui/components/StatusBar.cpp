#include "ui/components/StatusBar.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {

void StatusBar::draw(AppState& state, Viewport& viewport) {
    (void)viewport;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - UIStyle::statusH));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, UIStyle::statusH));
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

        std::string animName = "A person eating an apple (120 frames)";
        ImGui::TextDisabled("Loaded animation:");
        ImGui::SameLine(0, 6);
        ImGui::TextColored(UIStyle::text, "%s", animName.c_str());

        // Right side: Vulkan | 60 FPS | Kimodo Studio 0.1.0
        float rightW = 280.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - rightW);

        ImGui::TextDisabled("Vulkan");
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
