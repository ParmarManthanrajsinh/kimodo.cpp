#pragma once

#include "imgui.h"

namespace studio
{

// Centralized NVIDIA-grade dark theme + dense editor dimensions & colors.
struct UIStyle
{
    static constexpr float top_h = 46.0f;
    static constexpr float timeline_h = 84.0f;
    static constexpr float status_h = 26.0f;
    static constexpr float sideMin = 132.0f;
    static constexpr float sideMax = 180.0f;
    static constexpr float panelMin = 260.0f;
    static constexpr float panelMax = 340.0f;
    static constexpr float side_w = 168.0f;
    static constexpr float panel_w = 310.0f;
    static constexpr float rounding = 4.0f;

    static inline const ImVec4 accent = ImVec4(0.28f, 0.82f, 0.28f, 1.0f);
    static inline const ImVec4 bg = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    static inline const ImVec4 panel = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    static inline const ImVec4 card = ImVec4(0.12f, 0.12f, 0.15f, 1.0f);
    static inline const ImVec4 text = ImVec4(0.92f, 0.93f, 0.95f, 1.0f);
    static inline const ImVec4 text_muted = ImVec4(0.55f, 0.58f, 0.65f, 1.0f);
    static inline const ImVec4 green = ImVec4(0.30f, 0.85f, 0.35f, 1.0f);
    static inline const ImVec4 yellow = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
    static inline const ImVec4 red = ImVec4(0.95f, 0.30f, 0.30f, 1.0f);
};

struct Theme
{
    static void Apply();
    // Section header: small caps label + thin rule.
    static void section_header(const char* label);
    // Status pill: colored dot + text, no reliance on color alone.
    static void StatusBadge(bool ok, const char* ok_text, const char* warn_text);
    static ImVec4 accent();
    static ImVec4 accent_dim();
    static ImVec4 accent_bright();
    static ImVec4 bg_dark();
    static ImVec4 bg_panel();
    static ImVec4 bg_card();
    // Draw vector Kimodo origami polygon logo.
    static void DrawKimodoLogo(float x, float y, float size);
};

} // namespace studio
