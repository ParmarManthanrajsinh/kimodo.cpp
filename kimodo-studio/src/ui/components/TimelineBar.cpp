#include "ui/components/TimelineBar.h"
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

namespace studio {

void TimelineBar::draw(AppState& state, AnimationPlayer& player, float panelWidth) {
    (void)state;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float timelineY = vp->Pos.y + vp->Size.y - UIStyle::statusH - UIStyle::timelineH;
    float timelineW = vp->Size.x - UIStyle::sideW - panelWidth;

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + UIStyle::sideW, timelineY));
    ImGui::SetNextWindowSize(ImVec2(timelineW, UIStyle::timelineH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIStyle::panel);

    if (ImGui::Begin("##TimelineBar", nullptr, flags)) {
        // Transport Controls
        if (ImGui::Button(icons::kStepBack)) {
            player.stepFrame(-1);
        }
        ImGui::SameLine();

        const char* playIcon = player.isPlaying() ? icons::kPause : icons::kPlay;
        if (ImGui::Button(playIcon, ImVec2(36, 0))) {
            player.togglePlay();
        }
        ImGui::SameLine();

        if (ImGui::Button(icons::kStepForward)) {
            player.stepFrame(1);
        }
        ImGui::SameLine(0, 16);

        // Loop Toggle
        bool loop = player.isLooping();
        if (ImGui::Checkbox(ICON_FA_REPEAT " Loop", &loop)) {
            player.setLoop(loop);
        }

        ImGui::SameLine(0, 20);
        // Frame readout
        const int curFrame = player.frame();
        const int totalFrames = std::max(1, player.totalFrames());
        ImGui::Text("%03d / %03d", curFrame, totalFrames);

        ImGui::SameLine(0, 16);
        // Timeline Scrubber Slider
        float curTime = player.time();
        const float maxTime = std::max(0.01f, player.duration());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
        if (ImGui::SliderFloat("##TimeScrubber", &curTime, 0.0f, maxTime, "%.2fs")) {
            player.scrub(curTime);
        }

        ImGui::SameLine(0, 16);
        // FPS readout
        ImGui::TextDisabled("%.0f FPS", player.fps());
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace studio
