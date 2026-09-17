#include "ui/components/TimelineBar.h"
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace studio {

void TimelineBar::draw(AppState& state, AnimationPlayer& player, float panelWidth) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float sideW = state.sideWidth;
    const float viewportW = vp->Size.x - sideW - panelWidth;

    // Floating pill container centered horizontally and responsive to window width
    const float barW = std::clamp(viewportW - 32.0f, 480.0f, 1200.0f);
    const float barH = 78.0f;
    const float barX = vp->Pos.x + sideW + (viewportW - barW) * 0.5f;
    const float barY = vp->Pos.y + vp->Size.y - UIStyle::statusH - barH - 12.0f;

    ImGui::SetNextWindowPos(ImVec2(barX, barY));
    ImGui::SetNextWindowSize(ImVec2(barW, barH));
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
        const int curFrame = player.frame();
        const int totalFrames = std::max(1, player.totalFrames());
        const float curTime = player.time();
        const float maxTime = std::max(0.01f, player.duration());
        const float ctrlH = 26.0f;

        // ==========================================
        // 1. TOP CONTROLS ROW
        // ==========================================
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(UIStyle::text, "TIMELINE");
        ImGui::SameLine(0, 14);

        // First frame |<<
        if (ImGui::Button(icons::kFirst, ImVec2(26, ctrlH))) {
            player.restart();
        }
        ImGui::SameLine();

        // Circular Green Play / Pause Button
        {
            bool isPlaying = player.isPlaying();
            ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ? ImVec4(0.18f, 0.22f, 0.28f, 1.0f) : ImVec4(0.13f, 0.77f, 0.37f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isPlaying ? ImVec4(0.24f, 0.28f, 0.36f, 1.0f) : ImVec4(0.16f, 0.85f, 0.42f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 13.0f);

            if (ImGui::Button(isPlaying ? icons::kPause : icons::kPlay, ImVec2(26, ctrlH))) {
                player.togglePlay();
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::SameLine();

        // Last frame >>|
        if (ImGui::Button(icons::kLast, ImVec2(26, ctrlH))) {
            player.seekFrame(totalFrames - 1);
        }
        ImGui::SameLine();

        // Loop toggle pill switch
        bool isLoop = player.isLooping();
        if (isLoop) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.28f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::accent);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.70f, 0.78f, 1.0f));
        }
        if (ImGui::Button(isLoop ? "● Loop" : "○ Loop", ImVec2(58, ctrlH))) {
            player.setLoop(!isLoop);
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 14);

        // Frame readout (e.g. "Frame: 065 / 120")
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(UIStyle::text, "Frame: %03d / %d", curFrame, totalFrames);

        // Time readout (e.g. "Time: 00:02.17 / 00:04.00")
        ImGui::SameLine(0, 12);
        ImGui::AlignTextToFramePadding();
        int curMins = static_cast<int>(curTime) / 60;
        float curSecs = curTime - curMins * 60;
        int maxMins = static_cast<int>(maxTime) / 60;
        float maxSecs = maxTime - maxMins * 60;
        char timeBuf[64];
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%05.2f / %02d:%05.2f",
                      curMins, curSecs, maxMins, maxSecs);
        ImGui::TextDisabled("Time: %s", timeBuf);

        // Right-side controls (FPS dropdown, Playback speed)
        const float rightControlsWidth = 160.0f;
        if (ImGui::GetContentRegionAvail().x > rightControlsWidth) {
            ImGui::SameLine(ImGui::GetWindowWidth() - rightControlsWidth - 14.0f);
        }

        // FPS selector
        ImGui::SetNextItemWidth(74);
        static const char* fpsOptions[] = {"FPS 24", "FPS 30", "FPS 60"};
        int fpsIdx = (state.targetFps == 60) ? 2 : ((state.targetFps == 24) ? 0 : 1);
        if (ImGui::Combo("##FpsCombo", &fpsIdx, fpsOptions, IM_ARRAYSIZE(fpsOptions))) {
            state.targetFps = (fpsIdx == 2) ? 60 : ((fpsIdx == 0) ? 24 : 30);
        }

        ImGui::SameLine();
        // Speed selector
        ImGui::SetNextItemWidth(68);
        static const char* speedLabels[] = {"0.25x", "0.5x", "1.0x", "1.5x", "2.0x"};
        static const float speedValues[] = {0.25f, 0.5f, 1.0f, 1.5f, 2.0f};
        int speedIdx = 2; // Default 1.0x
        for (int i = 0; i < 5; ++i) {
            if (std::abs(state.playbackSpeed - speedValues[i]) < 0.05f) {
                speedIdx = i;
                break;
            }
        }
        if (ImGui::Combo("##SpeedCombo", &speedIdx, speedLabels, IM_ARRAYSIZE(speedLabels))) {
            state.playbackSpeed = speedValues[speedIdx];
        }

        // ==========================================
        // 2. BOTTOM SCRUBBER TRACK WITH TICKS
        // ==========================================
        ImGui::Spacing();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 trackPos = ImGui::GetCursorScreenPos();
        float availW = ImGui::GetContentRegionAvail().x;
        float trackH = 4.0f;

        // Interactive button over the scrubber
        ImGui::InvisibleButton("##ScrubberInteractive", ImVec2(availW, 20.0f));
        bool scrubHovered = ImGui::IsItemHovered();
        bool scrubActive = ImGui::IsItemActive();

        if (scrubHovered || scrubActive) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if (scrubActive) {
            float mouseX = ImGui::GetIO().MousePos.x;
            float frac = std::clamp((mouseX - trackPos.x) / availW, 0.0f, 1.0f);
            int scrubbedFrame = static_cast<int>(frac * (totalFrames - 1));
            player.seekFrame(scrubbedFrame);
        }

        // Track Background
        ImVec2 tMin = ImVec2(trackPos.x, trackPos.y + 4.0f);
        ImVec2 tMax = ImVec2(trackPos.x + availW, trackPos.y + 4.0f + trackH);
        draw->AddRectFilled(tMin, tMax, IM_COL32(36, 42, 54, 255), 2.0f);

        // Progress Fill
        float progressFrac = (totalFrames > 1) ? (static_cast<float>(curFrame) / (totalFrames - 1)) : 0.0f;
        progressFrac = std::clamp(progressFrac, 0.0f, 1.0f);
        float fillX = trackPos.x + progressFrac * availW;
        if (fillX > tMin.x) {
            draw->AddRectFilled(tMin, ImVec2(fillX, tMax.y), IM_COL32(34, 197, 94, 255), 2.0f);
        }

        // Playhead Scrubber Handle (Green pill on track + vertical green line)
        draw->AddLine(ImVec2(fillX, trackPos.y - 2.0f), ImVec2(fillX, tMax.y + 12.0f), IM_COL32(34, 197, 94, 255), 1.5f);
        draw->AddRectFilled(ImVec2(fillX - 3.5f, trackPos.y + 1.0f), ImVec2(fillX + 3.5f, tMax.y + 3.0f), IM_COL32(34, 197, 94, 255), 3.0f);

        // Frame Ticks & Labels below track (0, 10, 20 ... 120)
        int tickStep = (totalFrames > 150) ? 20 : (totalFrames > 60 ? 10 : 5);
        for (int f = 0; f < totalFrames; f += tickStep) {
            float fFrac = static_cast<float>(f) / static_cast<float>(totalFrames - 1);
            float tx = trackPos.x + fFrac * availW;
            draw->AddLine(ImVec2(tx, tMax.y + 2.0f), ImVec2(tx, tMax.y + 6.0f), IM_COL32(90, 100, 120, 180), 1.0f);

            char tickText[16];
            std::snprintf(tickText, sizeof(tickText), "%d", f);
            draw->AddText(ImVec2(tx - 4.0f, tMax.y + 7.0f), IM_COL32(100, 110, 130, 200), tickText);
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(2);
}

} // namespace studio
