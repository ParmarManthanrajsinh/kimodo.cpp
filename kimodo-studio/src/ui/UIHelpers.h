#pragma once

#include "imgui.h"
#include "ui/Theme.h"

#include <functional>

namespace studio::ui {

/// Draw a section header with optional description
inline void DrawSectionHeader(const char* title, const char* desc = nullptr) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(UIStyle::text, "%s", title);
    if (desc && desc[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", desc);
    }
    ImGui::Spacing();
}

/// Draw a standardized panel header with optional status badge
inline void DrawPanelHeader(const char* title, const char* badge = nullptr, const ImVec4& badgeCol = UIStyle::accent) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(UIStyle::text, "%s", title);
    if (badge && badge[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextColored(badgeCol, "%s", badge);
    }
    ImGui::Spacing();
}

/// Draw a property row: [Label] .............. [Controls]
/// Automatically aligns label baseline with control frame padding.
inline void DrawPropertyRow(const char* label, const std::function<void(float avail_width)>& draw_controls,
                            float label_width = 72.0f) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(label_width);
    float avail_w = ImGui::GetContentRegionAvail().x;
    draw_controls(avail_w);
}

/// Draw a consistent button with shared height and aligned text
inline bool DrawAlignedButton(const char* label, const ImVec2& size = ImVec2(0, 0), bool active = false) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.85f, 0.44f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.28f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::text);
    }

    ImVec2 actual_size = size;
    if (actual_size.y <= 0.0f) {
        actual_size.y = ImGui::GetFrameHeight();
    }

    bool clicked = ImGui::Button(label, actual_size);
    ImGui::PopStyleColor(3);
    return clicked;
}

} // namespace studio::ui
