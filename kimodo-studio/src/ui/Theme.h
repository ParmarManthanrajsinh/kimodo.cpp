#pragma once

#include <string>

struct ImVec4;

namespace studio {

// Centralized NVIDIA-grade dark theme + dense editor dimensions.
// Single source of truth, no magic numbers in pages.
struct UIStyle {
    static constexpr float topH = 46.0f;
    static constexpr float timelineH = 84.0f;
    static constexpr float statusH = 26.0f;
    static constexpr float sideMin = 132.0f;
    static constexpr float sideMax = 180.0f;
    static constexpr float panelMin = 260.0f;
    static constexpr float panelMax = 340.0f;
    static constexpr float sideW = 168.0f;
    static constexpr float panelW = 310.0f;
    static constexpr float rounding = 4.0f;
};

struct Theme {
    static void apply();
    // Section header: small caps label + thin rule.
    static void sectionHeader(const char* label);
    // Status pill: colored dot + text, no reliance on color alone.
    static void statusBadge(bool ok, const char* okText, const char* warnText);
    static ImVec4 accent();
    static ImVec4 accentDim();
    static ImVec4 accentBright();
    static ImVec4 bgDark();
    static ImVec4 bgPanel();
    static ImVec4 bgCard();
    // Draw vector Kimodo origami polygon logo.
    static void drawKimodoLogo(float x, float y, float size);
};

} // namespace studio
