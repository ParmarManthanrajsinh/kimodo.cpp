#include "ui/Toast.h"

#include "imgui.h"
#include "raylib.h"

#include <algorithm>

namespace studio {

void Toasts::push(const std::string& text, ToastKind kind) {
    items_.push_back({text, kind, GetTime() + 4.0});
}

void Toasts::draw() {
    const double now = GetTime();
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [now](const Toast& t) { return t.expiresAt < now; }),
                 items_.end());

    // Bottom-left, above the transport bar: never under the right panel.
    // Drawn after all other windows, without focus-stealing flags.
    const float baseY = (float)GetScreenHeight() - 64.0f - 12.0f;
    float y = baseY;
    int idx = 0;
    for (auto it = items_.rbegin(); it != items_.rend(); ++it) {
        const Toast& t = *it;
        ImVec4 color;
        const char* icon = "";
        switch (t.kind) {
            case ToastKind::Success: color = ImVec4(0.30f, 0.85f, 0.45f, 1.0f); icon = "[ok] "; break;
            case ToastKind::Warning: color = ImVec4(0.95f, 0.75f, 0.25f, 1.0f); icon = "[!] "; break;
            case ToastKind::Error: color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f); icon = "[x] "; break;
            default: color = ImVec4(0.55f, 0.70f, 1.0f, 1.0f); icon = "[i] "; break;
        }
        std::string label = "##toast" + std::to_string(idx++);
        ImGui::SetNextWindowPos(ImVec2(160.0f, y), ImGuiCond_Always, ImVec2(0, 1));
        ImGui::SetNextWindowSize(ImVec2(330.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, color);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::Begin(label.c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::TextColored(color, "%s%s", icon, t.text.c_str());
        const float h = ImGui::GetWindowHeight();
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        y -= (h > 8.0f ? h : 48.0f) + 8.0f;
    }
}

} // namespace studio
