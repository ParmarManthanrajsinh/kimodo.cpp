#include "ui/components/TimelineBar.h"
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace studio {

void TimelineBar::Draw(AppState& state, AnimationPlayer& player, float panel_width) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float side_w = state.side_width;
    const float viewport_w = vp->Size.x - side_w - panel_width;

    // Floating pill container centered horizontally and responsive to window width
    const float bar_w = std::clamp(viewport_w - 32.0f, 480.0f, 1200.0f);
    const float bar_h = 78.0f;
    const float bar_x = vp->Pos.x + side_w + (viewport_w - bar_w) * 0.5f;
    const float bar_y = vp->Pos.y + vp->Size.y - UIStyle::status_h - bar_h - 12.0f;

    ImGui::SetNextWindowPos(ImVec2(bar_x, bar_y));
    ImGui::SetNextWindowSize(ImVec2(bar_w, bar_h));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

    if (ImGui::Begin("##FloatingTimelineBar", nullptr, flags)) {
        const int cur_frame = player.Frame();
        const int total_frames = std::max(1, player.GetTotalFrames());
        const float cur_time = player.GetTime();
        const float max_time = std::max(0.01f, player.GetDuration());
        const float ctrl_h = 26.0f;

        // ==========================================
        // 1. TOP CONTROLS ROW
        // ==========================================
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(UIStyle::text, "TIMELINE");
        ImGui::SameLine(0, 14);

        // First frame |<<
        if (ImGui::Button(icons::kFirst, ImVec2(26, ctrl_h))) {
            player.Restart();
        }
        ImGui::SameLine();

        // Circular Green Play / Pause Button
        {
            bool is_playing = player.IsPlaying();
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  is_playing ? ImVec4(0.18f, 0.22f, 0.28f, 1.0f) : ImVec4(0.13f, 0.77f, 0.37f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  is_playing ? ImVec4(0.24f, 0.28f, 0.36f, 1.0f) : ImVec4(0.16f, 0.85f, 0.42f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 13.0f);

            if (ImGui::Button(is_playing ? icons::kPause : icons::kPlay, ImVec2(26, ctrl_h))) {
                player.TogglePlay();
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::SameLine();

        // Last frame >>|
        if (ImGui::Button(icons::kLast, ImVec2(26, ctrl_h))) {
            player.SeekFrame(total_frames - 1);
        }
        ImGui::SameLine();

        // Loop toggle pill switch
        bool is_loop = player.IsLooping();
        if (is_loop) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.28f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::accent);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.70f, 0.78f, 1.0f));
        }
        if (ImGui::Button(is_loop ? "● Loop" : "○ Loop", ImVec2(58, ctrl_h))) {
            player.SetLoop(!is_loop);
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 14);

        // Frame readout (e.g. "Frame: 065 / 120")
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(UIStyle::text, "Frame: %03d / %d", cur_frame, total_frames);

        // Time readout (e.g. "Time: 00:02.17 / 00:04.00")
        ImGui::SameLine(0, 12);
        ImGui::AlignTextToFramePadding();
        int cur_mins = static_cast<int>(cur_time) / 60;
        float curSecs = cur_time - cur_mins * 60;
        int max_mins = static_cast<int>(max_time) / 60;
        float max_secs = max_time - max_mins * 60;
        char time_buf[64];
        std::snprintf(time_buf, sizeof(time_buf), "%02d:%05.2f / %02d:%05.2f", cur_mins, curSecs, max_mins, max_secs);
        ImGui::TextDisabled("Time: %s", time_buf);

        // Right-side controls (FPS dropdown, Playback speed)
        const float right_controls_width = 160.0f;
        if (ImGui::GetContentRegionAvail().x > right_controls_width) {
            ImGui::SameLine(ImGui::GetWindowWidth() - right_controls_width - 14.0f);
        }

        // FPS selector
        ImGui::SetNextItemWidth(74);
        static const char* fps_options[] = {"FPS 24", "FPS 30", "FPS 60"};
        int fps_idx = (state.target_fps == 60) ? 2 : ((state.target_fps == 24) ? 0 : 1);
        if (ImGui::Combo("##FpsCombo", &fps_idx, fps_options, IM_ARRAYSIZE(fps_options))) {
            state.target_fps = (fps_idx == 2) ? 60 : ((fps_idx == 0) ? 24 : 30);
        }

        ImGui::SameLine();
        // Speed selector
        ImGui::SetNextItemWidth(68);
        static const char* speed_labels[] = {"0.25x", "0.5x", "1.0x", "1.5x", "2.0x"};
        static const float speed_values[] = {0.25f, 0.5f, 1.0f, 1.5f, 2.0f};
        int speed_idx = 2; // Default 1.0x
        for (int i = 0; i < 5; ++i) {
            if (std::abs(state.playback_speed - speed_values[i]) < 0.05f) {
                speed_idx = i;
                break;
            }
        }
        if (ImGui::Combo("##SpeedCombo", &speed_idx, speed_labels, IM_ARRAYSIZE(speed_labels))) {
            state.playback_speed = speed_values[speed_idx];
        }

        // ==========================================
        // 2. BOTTOM SCRUBBER TRACK WITH TICKS
        // ==========================================
        ImGui::Spacing();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 track_pos = ImGui::GetCursorScreenPos();
        float avail_w = ImGui::GetContentRegionAvail().x;
        float track_h = 4.0f;

        // Interactive button over the scrubber
        ImGui::InvisibleButton("##ScrubberInteractive", ImVec2(avail_w, 20.0f));
        bool scrub_hovered = ImGui::IsItemHovered();
        bool scrub_active = ImGui::IsItemActive();

        if (scrub_hovered || scrub_active) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if (scrub_active) {
            float mouse_x = ImGui::GetIO().MousePos.x;
            float frac = std::clamp((mouse_x - track_pos.x) / avail_w, 0.0f, 1.0f);
            int scrubbed_frame = static_cast<int>(frac * (total_frames - 1));
            player.SeekFrame(scrubbed_frame);
        }

        // Track Background
        ImVec2 t_min = ImVec2(track_pos.x, track_pos.y + 4.0f);
        ImVec2 t_max = ImVec2(track_pos.x + avail_w, track_pos.y + 4.0f + track_h);
        draw->AddRectFilled(t_min, t_max, IM_COL32(36, 42, 54, 255), 2.0f);

        // Progress Fill
        float progress_frac = (total_frames > 1) ? (static_cast<float>(cur_frame) / (total_frames - 1)) : 0.0f;
        progress_frac = std::clamp(progress_frac, 0.0f, 1.0f);
        float fill_x = track_pos.x + progress_frac * avail_w;
        if (fill_x > t_min.x) {
            draw->AddRectFilled(t_min, ImVec2(fill_x, t_max.y), IM_COL32(34, 197, 94, 255), 2.0f);
        }

        // Playhead Scrubber Handle (Green pill on track + vertical green line)
        draw->AddLine(ImVec2(fill_x, track_pos.y - 2.0f), ImVec2(fill_x, t_max.y + 12.0f), IM_COL32(34, 197, 94, 255),
                      1.5f);
        draw->AddRectFilled(ImVec2(fill_x - 3.5f, track_pos.y + 1.0f), ImVec2(fill_x + 3.5f, t_max.y + 3.0f),
                            IM_COL32(34, 197, 94, 255), 3.0f);

        // Frame Ticks & Labels below track (0, 10, 20 ... 120)
        int tick_step = (total_frames > 150) ? 20 : (total_frames > 60 ? 10 : 5);
        for (int f = 0; f < total_frames; f += tick_step) {
            float f_frac = static_cast<float>(f) / static_cast<float>(total_frames - 1);
            float tx = track_pos.x + f_frac * avail_w;
            draw->AddLine(ImVec2(tx, t_max.y + 2.0f), ImVec2(tx, t_max.y + 6.0f), IM_COL32(90, 100, 120, 180), 1.0f);

            char tick_text[16];
            std::snprintf(tick_text, sizeof(tick_text), "%d", f);
            draw->AddText(ImVec2(tx - 4.0f, t_max.y + 7.0f), IM_COL32(100, 110, 130, 200), tick_text);
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(2);
}

} // namespace studio
