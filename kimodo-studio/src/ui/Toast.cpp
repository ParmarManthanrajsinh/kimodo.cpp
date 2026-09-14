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

    float y = 48.0f;
    for (const Toast& t : items_) {
        ImVec4 color;
        const char* icon = "";
        switch (t.kind) {
            case ToastKind::Success: color = ImVec4(0.30f, 0.85f, 0.45f, 1.0f); icon = "[ok] "; break;
            case ToastKind::Warning: color = ImVec4(0.95f, 0.75f, 0.25f, 1.0f); icon = "[!] "; break;
            case ToastKind::Error: color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f); icon = "[x] "; break;
            default: color = ImVec4(0.55f, 0.70f, 1.0f, 1.0f); icon = "[i] "; break;
        }
        std::string label = "##toast" + std::to_string(y);
        ImGui::SetNextWindowPos(ImVec2((float)GetScreenWidth() - 340.0f, y));
        ImGui::SetNextWindowSize(ImVec2(324.0f, 0.0f));
        ImGui::Begin(label.c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(color, "%s%s", icon, t.text.c_str());
        ImGui::End();
        y += 56.0f;
    }
}

} // namespace studio
