#pragma once

#include "imgui.h"
#include <string>

namespace studio {

// Centralized NVIDIA-grade dark theme + dense editor dimensions & colors.
struct FUIStyle {
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

    static inline const ImVec4 accent = ImVec4(0.28f, 0.82f, 0.28f, 1.0f);
    static inline const ImVec4 bg = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    static inline const ImVec4 panel = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    static inline const ImVec4 card = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
    static inline const ImVec4 text = ImVec4(0.92f, 0.93f, 0.95f, 1.0f);
    static inline const ImVec4 textMuted = ImVec4(0.55f, 0.58f, 0.65f, 1.0f);
    static inline const ImVec4 green = ImVec4(0.30f, 0.85f, 0.35f, 1.0f);
    static inline const ImVec4 yellow = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
    static inline const ImVec4 red = ImVec4(0.95f, 0.30f, 0.30f, 1.0f);
};

struct FTheme {
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
