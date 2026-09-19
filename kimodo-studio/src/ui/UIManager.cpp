#include "ui/UIManager.h"
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"

#include "ui/components/HeaderBar.h"
#include "ui/components/NavRail.h"
#include "ui/components/StatusBar.h"
#include "ui/components/TimelineBar.h"

#include "animation/AnimationPlayer.h"
#include "character/CharacterLibrary.h"
#include "rendering/Viewport.h"
#include "ui/pages/PageCharacters.h"
#include "ui/pages/PageExport.h"
#include "ui/pages/PageGenerate.h"
#include "ui/pages/PageHome.h"
#include "ui/pages/PageLibrary.h"
#include "ui/pages/PageModels.h"
#include "ui/pages/PageRetarget.h"
#include "ui/pages/PageSettings.h"

#include <ctime>

namespace studio
{

void UIManager::Shutdown()
{
    for (auto& [k, tex] : thumbs)
    {
        if (tex.id > 0)
        {
            UnloadTexture(tex);
            tex.id = 0;
        }
    }
    thumbs.clear();
}

void UIManager::Draw(AppState& state, Viewport& viewport, KimodoEngine& engine, AnimationPlayer& player,
                     AnimationLibrary& library, CharacterLibrary& characters, ModelManager& models, Toasts& toasts,
                     CaptureFn capture)
{
    (void)capture;
    ImGuiViewport* vp = ImGui::GetMainViewport();

    // 1. Draw Top Header Bar
    HeaderBar::Draw(state, engine, models);

    // 2. Draw Left Navigation Rail
    NavRail::Draw(state);

    // 3. Draw Right Active Tool / Workspace Panel
    float panel_w = std::clamp(state.panel_width, 260.0f, 640.0f);
    state.panel_width = panel_w;

    float panel_x = vp->Pos.x + vp->Size.x - panel_w;
    float panel_y = vp->Pos.y + UIStyle::top_h;
    float panel_h = vp->Size.y - UIStyle::top_h - UIStyle::status_h;

    ImGui::SetNextWindowPos(ImVec2(panel_x, panel_y));
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags panel_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIStyle::panel);

    if (ImGui::Begin("##ActivePagePanel", nullptr, panel_flags))
    {
        switch (state.screen)
        {
        case Screen::Home:
        case Screen::Characters:
            PageCharacters::Draw(state, characters, viewport, &player);
            break;
        case Screen::Generate:
            PageGenerate::Draw(state, engine, models, toasts);
            break;
        case Screen::Models:
            PageModels::Draw(state, models, toasts);
            break;
        case Screen::Library:
            PageLibrary::Draw(state, library, player, toasts);
            break;
        case Screen::Retarget:
            PageRetarget::Draw(state, library, player, toasts);
            break;
        case Screen::Export:
            PageExport::Draw(state, player, library, characters, toasts);
            break;
        case Screen::settings:
            PageSettings::Draw(state, viewport, toasts);
            break;
        default:
            PageCharacters::Draw(state, characters, viewport, &player);
            break;
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // 4. Draw Interactive Splitters (Draggable Side Panels)
    ImDrawList* fg_draw = ImGui::GetForegroundDrawList(vp);
    ImGuiIO& io = ImGui::GetIO();

    // Left Sidebar Splitter Handle
    float left_splitter_x = vp->Pos.x + state.side_width;
    ImGui::SetNextWindowPos(ImVec2(left_splitter_x - 3.0f, panel_y));
    ImGui::SetNextWindowSize(ImVec2(6.0f, panel_h));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags splitter_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##LeftSplitterWindow", nullptr, splitter_flags))
    {
        ImGui::InvisibleButton("##LeftSplitterBtn", ImVec2(6.0f, panel_h));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        if (hovered || active)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (active)
        {
            state.side_width += io.MouseDelta.x;
            state.side_width = std::clamp(state.side_width, 120.0f, 320.0f);
        }
        ImU32 col = active ? ImGui::GetColorU32(UIStyle::accent)
                           : (hovered ? IM_COL32(100, 200, 120, 220) : IM_COL32(40, 44, 52, 160));
        fg_draw->AddLine(ImVec2(left_splitter_x, panel_y), ImVec2(left_splitter_x, panel_y + panel_h), col,
                         (hovered || active) ? 2.0f : 1.0f);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Right Workspace Panel Splitter Handle
    float right_splitter_x = panel_x;
    ImGui::SetNextWindowPos(ImVec2(right_splitter_x - 3.0f, panel_y));
    ImGui::SetNextWindowSize(ImVec2(6.0f, panel_h));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("##RightSplitterWindow", nullptr, splitter_flags))
    {
        ImGui::InvisibleButton("##RightSplitterBtn", ImVec2(6.0f, panel_h));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        if (hovered || active)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (active)
        {
            state.panel_width -= io.MouseDelta.x;
            state.panel_width = std::clamp(state.panel_width, 260.0f, 640.0f);
        }
        ImU32 col = active ? ImGui::GetColorU32(UIStyle::accent)
                           : (hovered ? IM_COL32(100, 200, 120, 220) : IM_COL32(40, 44, 52, 160));
        fg_draw->AddLine(ImVec2(right_splitter_x, panel_y), ImVec2(right_splitter_x, panel_y + panel_h), col,
                         (hovered || active) ? 2.0f : 1.0f);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // 5. Draw Central Editor Viewport (Raylib-ImGui-Hybrid Image)
    float viewport_x = vp->Pos.x + state.side_width;
    float viewport_y = vp->Pos.y + UIStyle::top_h;
    float viewport_w = std::max(1.0f, vp->Size.x - state.side_width - state.panel_width);
    float viewport_h = std::max(1.0f, vp->Size.y - UIStyle::top_h - UIStyle::status_h);

    ImGui::SetNextWindowPos(ImVec2(viewport_x, viewport_y));
    ImGui::SetNextWindowSize(ImVec2(viewport_w, viewport_h));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags viewport_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.09f, 1.0f));

    if (ImGui::Begin("##CentralViewportWindow", nullptr, viewport_flags))
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x >= 1.0f && avail.y >= 1.0f)
        {
            state.desired_viewport_width = static_cast<int>(avail.x);
            state.desired_viewport_height = static_cast<int>(avail.y);
        }

        if (viewport.IsReady())
        {
            ImTextureID tex_id = (ImTextureID)(intptr_t)viewport.GetTextureId();
            ImGui::Image(tex_id, avail, ImVec2(0, 1), ImVec2(1, 0));
            state.viewport_hovered = ImGui::IsItemHovered() || ImGui::IsWindowHovered();
            ImVec2 mpos = ImGui::GetMousePos();
            ImVec2 img_min = ImGui::GetItemRectMin();
            state.viewport_mouse_pos = Vector2{mpos.x - img_min.x, mpos.y - img_min.y};

            bool is_mouse_down = (ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                                  ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                                  ImGui::IsMouseDown(ImGuiMouseButton_Right));
            bool can_interact = state.viewport_hovered || (state.viewport_dragging && is_mouse_down);
            state.viewport_dragging = can_interact && is_mouse_down;
            viewport.Update(!can_interact);
        }
        else
        {
            state.viewport_hovered = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // 6. Draw Viewport Floating Overlays (Toolbar & HUDs)

    // 5a. Top Floating Viewport Toolbar
    {
        float max_tool_w = std::max(200.0f, (right_splitter_x - viewport_x) - 180.0f);
        float tool_w = std::min(570.0f, max_tool_w);
        float tool_h = 36.0f;
        float tool_x = viewport_x + 14.0f;
        float tool_y = vp->Pos.y + UIStyle::top_h + 12.0f;

        ImGui::SetNextWindowPos(ImVec2(tool_x, tool_y));
        ImGui::SetNextWindowSize(ImVec2(tool_w, tool_h));
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags tool_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoScrollbar;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.12f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.24f, 0.32f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        if (ImGui::Begin("##ViewportTopToolbar", nullptr, tool_flags))
        {
            const float item_h = ImGui::GetFrameHeight();

            // Camera Projection Dropdown
            ImGui::SetNextItemWidth(102);
            static const char* projLabels[] = {"Perspective", "Orthographic"};
            if (ImGui::Combo("##CameraProjCombo", &state.camera_projection, projLabels, 2))
            {
                viewport.SetProjection(state.camera_projection);
            }

            ImGui::SameLine(0, 8);

            // Display Toggles (Grid, Axes, Floor, Skeleton, Wireframe)
            auto draw_toggle_btn = [item_h](const char* label, bool& val, bool has_plus = true) {
                std::string text = (has_plus && val ? "+ " : (has_plus ? "+ " : "")) + std::string(label);
                if (val)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.28f, 0.18f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::accent);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.70f, 0.78f, 1.0f));
                }
                if (ImGui::Button(text.c_str(), ImVec2(0, item_h)))
                {
                    val = !val;
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine(0, 5);
            };

            draw_toggle_btn("Grid", state.show_grid, true);
            draw_toggle_btn("Axes", state.show_axes, true);
            draw_toggle_btn("Floor", state.show_floor, true);
            draw_toggle_btn("Skeleton", state.show_skeleton, true);
            draw_toggle_btn("Wireframe", state.show_wireframe, false);

            ImGui::SameLine(0, 8);

            // Camera Snapshot & Reset
            if (ImGui::Button(ICON_FA_CAMERA "##Snap1", ImVec2(28, item_h)))
            {
                std::string snap_path = "screenshot_" + std::to_string(std::time(nullptr)) + ".png";
                TakeScreenshot(snap_path.c_str());
                toasts.Push("Snapshot saved: " + snap_path, ToastKind::Success);
            }
            ImGui::SameLine(0, 4);
            if (ImGui::Button(ICON_FA_RESET " Reset", ImVec2(0, item_h)))
            {
                viewport.Reset();
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(5);
        ImGui::PopStyleColor(2);
    }

    // 5b. Top-Right Stats HUD (Skeleton: 30 joints | Vertices: 12,842 | FPS: 60)
    {
        float hud_w = 150.0f;
        float hud_h = 74.0f;
        float hud_x = right_splitter_x - hud_w - 14.0f;
        float hud_y = vp->Pos.y + UIStyle::top_h + 12.0f;

        ImGui::SetNextWindowPos(ImVec2(hud_x, hud_y));
        ImGui::SetNextWindowSize(ImVec2(hud_w, hud_h));
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags hud_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

        if (ImGui::Begin("##ViewportStatsHUD", nullptr, hud_flags))
        {
            CharacterAsset* active_char = characters.GetActiveAsset();
            int vert_count = 12842;
            int joint_count = 30;
            if (active_char && active_char->IsLoaded())
            {
                vert_count = static_cast<int>(active_char->GetSkinningData().vertices.size());
                joint_count = static_cast<int>(active_char->GetBones().size());
            }

            ImGui::TextDisabled("Skeleton:");
            ImGui::SameLine(64.0f);
            ImGui::TextColored(UIStyle::text, "%d joints", joint_count);

            ImGui::TextDisabled("Vertices:");
            ImGui::SameLine(64.0f);
            ImGui::TextColored(UIStyle::text, "%d", vert_count);

            ImGui::TextDisabled("FPS:");
            ImGui::SameLine(64.0f);
            ImGui::TextColored(UIStyle::text, "%d", state.fps > 0 ? state.fps : 60);
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    // 5c. Bottom-Right Animation HUD (Animation Name, Frame, Time, Playback)
    {
        float bar_h = 76.0f;
        float bar_y = vp->Pos.y + vp->Size.y - UIStyle::status_h - bar_h - 12.0f;
        float hud_w = 230.0f;
        float hud_h = 92.0f;
        float hud_x = right_splitter_x - hud_w - 14.0f;
        float hud_y = bar_y - hud_h - 10.0f;

        ImGui::SetNextWindowPos(ImVec2(hud_x, hud_y));
        ImGui::SetNextWindowSize(ImVec2(hud_w, hud_h));
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags hud_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

        if (ImGui::Begin("##ViewportPlaybackHUD", nullptr, hud_flags))
        {
            const int cur_frame = player.Frame();
            const int total_frames = std::max(1, player.GetTotalFrames());
            const float cur_time = player.GetTime();

            ImGui::TextDisabled("Animation:");
            ImGui::SameLine(72.0f);
            std::string anim_title = "A person eating an apple";
            ImGui::TextColored(UIStyle::text, "%s", anim_title.c_str());

            ImGui::TextDisabled("Frame:");
            ImGui::SameLine(72.0f);
            ImGui::TextColored(UIStyle::text, "%03d / %d", cur_frame, total_frames);

            ImGui::TextDisabled("Time:");
            ImGui::SameLine(72.0f);
            ImGui::TextColored(UIStyle::text, "%.2fs / 4.000", cur_time);

            ImGui::TextDisabled("Playback:");
            ImGui::SameLine(72.0f);
            ImGui::TextColored(UIStyle::text, "%.1fx", state.playback_speed);
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    // 6. Draw Bottom Timeline Bar
    TimelineBar::Draw(state, player, panel_w);

    // 6. Draw Bottom Status Bar
    StatusBar::Draw(state, viewport);

    // 7. Draw Toast Notifications Floating Overlay
    toasts.Draw();
}

} // namespace studio
