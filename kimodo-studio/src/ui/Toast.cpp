#include "ui/Toast.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include "imgui.h"
#include "imgui_internal.h"
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

    if (items_.empty()) return;

    // Position above timeline + status bar, to the right of the left sidebar.
    const float baseY = (float)GetScreenHeight() - (UIStyle::timelineH + UIStyle::statusH) - 16.0f;
    const float baseX = UIStyle::sideW + 16.0f;
    float y = baseY;
    int idx = 0;
    for (auto it = items_.rbegin(); it != items_.rend(); ++it) {
        const Toast& t = *it;
        ImVec4 color;
        const char* icon = "";
        switch (t.kind) {
            case ToastKind::Success:
                color = ImVec4(0.30f, 0.85f, 0.45f, 1.0f);
                icon = studio::icons::kCheckCircle;
                break;
            case ToastKind::Warning:
                color = ImVec4(0.95f, 0.75f, 0.25f, 1.0f);
                icon = studio::icons::kWarn;
                break;
            case ToastKind::Error:
                color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
                icon = studio::icons::kWarn;
                break;
            default:
                color = ImVec4(0.55f, 0.70f, 1.0f, 1.0f);
                icon = studio::icons::kEye;
                break;
        }
        std::string label = "##toast" + std::to_string(idx++);
        ImGui::SetNextWindowPos(ImVec2(baseX, y), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.14f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Border, color);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
        ImGui::Begin(label.c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImGui::TextColored(color, "%s", icon);
        ImGui::SameLine(0, 8.0f);
        ImGui::TextColored(ImVec4(0.92f, 0.92f, 0.95f, 1.0f), "%s", t.text.c_str());
        const float h = ImGui::GetWindowHeight();
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        y -= (h > 8.0f ? h : 44.0f) + 8.0f;
    }
}

} // namespace studio
