#include "ui/Theme.h"

#include "imgui.h"

namespace studio {

ImVec4 Theme::accent() {
    return ImVec4(0.28f, 0.82f, 0.28f, 1.0f); // Vibrant Studio Green #47d147
}

ImVec4 Theme::accent_dim() { return ImVec4(0.28f, 0.82f, 0.28f, 0.22f); }

ImVec4 Theme::accent_bright() { return ImVec4(0.35f, 0.95f, 0.35f, 1.0f); }

ImVec4 Theme::bg_dark() { return ImVec4(0.06f, 0.06f, 0.07f, 1.0f); }

ImVec4 Theme::bg_panel() { return ImVec4(0.08f, 0.08f, 0.10f, 1.0f); }

ImVec4 Theme::bg_card() { return ImVec4(0.12f, 0.12f, 0.15f, 1.0f); }

void Theme::DrawKimodoLogo(float x, float y, float size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (!dl)
        return;

    // Origami stylized polygon / dragon crest
    // Normalized coords [0, 1] relative to (x, y, size, size)
    auto P = [&](float px, float py) -> ImVec2 { return ImVec2(x + px * size, y + py * size); };

    const ImU32 col_outline = IM_COL32(75, 230, 75, 255);
    const ImU32 col_facet1 = IM_COL32(40, 160, 40, 230);
    const ImU32 col_facet2 = IM_COL32(65, 210, 65, 245);
    const ImU32 col_facet3 = IM_COL32(30, 130, 30, 220);
    const ImU32 col_facet4 = IM_COL32(85, 240, 85, 255);

    // Facet A: Top left horn/crest
    ImVec2 p0 = P(0.15f, 0.25f);
    ImVec2 p1 = P(0.50f, 0.08f);
    ImVec2 p2 = P(0.42f, 0.45f);
    dl->AddTriangleFilled(p0, p1, p2, col_facet2);

    // Facet B: Top right horn/crest
    ImVec2 p3 = P(0.85f, 0.25f);
    dl->AddTriangleFilled(p1, p3, p2, col_facet1);

    // Facet C: Left lower cheek/jaw
    ImVec2 p4 = P(0.20f, 0.72f);
    ImVec2 p5 = P(0.50f, 0.92f);
    dl->AddTriangleFilled(p0, p2, p4, col_facet3);

    // Facet D: Center snout
    dl->AddTriangleFilled(p2, p4, p5, col_facet4);

    // Facet E: Right lower cheek/jaw
    ImVec2 p6 = P(0.80f, 0.72f);
    dl->AddTriangleFilled(p2, p5, p6, col_facet2);
    dl->AddTriangleFilled(p2, p6, p3, col_facet1);

    // Clean wireframe / origami crease lines
    dl->AddLine(p0, p1, col_outline, 1.8f);
    dl->AddLine(p1, p3, col_outline, 1.8f);
    dl->AddLine(p3, p6, col_outline, 1.8f);
    dl->AddLine(p6, p5, col_outline, 1.8f);
    dl->AddLine(p5, p4, col_outline, 1.8f);
    dl->AddLine(p4, p0, col_outline, 1.8f);

    // Inner crease folds
    dl->AddLine(p1, p2, col_outline, 1.4f);
    dl->AddLine(p2, p5, col_outline, 1.4f);
    dl->AddLine(p0, p2, col_outline, 1.2f);
    dl->AddLine(p3, p2, col_outline, 1.2f);
    dl->AddLine(p4, p2, col_outline, 1.2f);
    dl->AddLine(p6, p2, col_outline, 1.2f);
}

void Theme::Apply() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = UIStyle::rounding;
    s.ChildRounding = UIStyle::rounding;
    s.FrameRounding = UIStyle::rounding;
    s.PopupRounding = UIStyle::rounding;
    s.ScrollbarRounding = UIStyle::rounding;
    s.GrabRounding = UIStyle::rounding;
    s.TabRounding = UIStyle::rounding;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.WindowPadding = ImVec2(10, 8);
    s.FramePadding = ImVec2(8, 5);
    s.CellPadding = ImVec2(6, 4);
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.IndentSpacing = 16.0f;
    s.ScrollbarSize = 10.0f;
    s.GrabMinSize = 10.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.51f, 0.56f, 1.0f);
    c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
    c[ImGuiCol_Border] = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
    c[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.07f, 0.07f, 0.08f, 1.0f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.26f, 0.26f, 0.30f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
    c[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.82f, 0.28f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.32f, 0.32f, 0.36f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.28f, 0.82f, 0.28f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.13f, 0.13f, 0.16f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.20f, 0.18f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.35f, 0.18f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.13f, 0.13f, 0.16f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.22f, 0.16f, 1.0f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.32f, 0.18f, 1.0f);
    c[ImGuiCol_Separator] = ImVec4(0.16f, 0.16f, 0.19f, 1.0f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.28f, 0.28f, 0.32f, 1.0f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.28f, 0.82f, 0.28f, 1.0f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.28f, 0.82f, 0.28f, 1.0f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.28f, 0.82f, 0.28f, 1.0f);
    c[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
    c[ImGuiCol_TabHovered] = ImVec4(0.16f, 0.18f, 0.14f, 1.0f);
    c[ImGuiCol_TabActive] = ImVec4(0.12f, 0.14f, 0.11f, 1.0f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.07f, 0.09f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.10f, 0.11f, 0.09f, 1.0f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.16f, 0.16f, 0.19f, 1.0f);
    c[ImGuiCol_TableBorderLight] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.28f, 0.82f, 0.28f, 0.35f);
}

void Theme::section_header(const char* label) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.90f, 1.0f), "%s", label);
    ImGui::Spacing();
}

void Theme::StatusBadge(bool ok, const char* ok_text, const char* warn_text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ok ? ImVec4(0.28f, 0.82f, 0.28f, 1.0f) : ImVec4(0.95f, 0.65f, 0.15f, 1.0f));
    ImGui::Bullet();
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted(ok ? ok_text : warn_text);
}

} // namespace studio
