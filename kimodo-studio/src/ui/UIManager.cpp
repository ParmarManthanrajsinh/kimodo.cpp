#include "ui/UIManager.h"
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"

#include "ui/components/HeaderBar.h"
#include "ui/components/NavRail.h"
#include "ui/components/StatusBar.h"
#include "ui/components/TimelineBar.h"

#include "ui/pages/PageCharacters.h"
#include "ui/pages/PageExport.h"
#include "ui/pages/PageGenerate.h"
#include "ui/pages/PageHome.h"
#include "ui/pages/PageLibrary.h"
#include "ui/pages/PageModels.h"
#include "ui/pages/PageRetarget.h"
#include "ui/pages/PageSettings.h"
#include "animation/AnimationPlayer.h"
#include "character/CharacterLibrary.h"
#include "rendering/Viewport.h"

#include <ctime>

namespace studio {

void UIManager::shutdown() {
    for (auto& [k, tex] : thumbs_) {
        if (tex.id > 0) {
            UnloadTexture(tex);
            tex.id = 0;
        }
    }
    thumbs_.clear();
}

void UIManager::draw(AppState& state, Viewport& viewport, KimodoEngine& engine,
                     AnimationPlayer& player, AnimationLibrary& library,
                     CharacterLibrary& characters, ModelManager& models,
                     Toasts& toasts, CaptureFn capture) {
    (void)capture;
    ImGuiViewport* vp = ImGui::GetMainViewport();

    // 1. Draw Top Header Bar
    HeaderBar::draw(state, engine, models);

    // 2. Draw Left Navigation Rail
    NavRail::draw(state);

    // 3. Draw Right Active Tool / Workspace Panel
    float panelW = std::clamp(state.panelWidth, 260.0f, 640.0f);
    state.panelWidth = panelW;

    float panelX = vp->Pos.x + vp->Size.x - panelW;
    float panelY = vp->Pos.y + UIStyle::topH;
    float panelH = vp->Size.y - UIStyle::topH - UIStyle::statusH;

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UIStyle::panel);

    if (ImGui::Begin("##ActivePagePanel", nullptr, panelFlags)) {
        switch (state.screen) {
            case Screen::Home:
            case Screen::Characters:
                PageCharacters::draw(state, characters, viewport, &player);
                break;
            case Screen::Generate:
                PageGenerate::draw(state, engine, models, toasts);
                break;
            case Screen::Models:
                PageModels::draw(state, models, toasts);
                break;
            case Screen::Library:
                PageLibrary::draw(state, library, player, toasts);
                break;
            case Screen::Retarget:
                PageRetarget::draw(state, library, player, toasts);
                break;
            case Screen::Export:
                PageExport::draw(state, player, library, characters, toasts);
                break;
            case Screen::Settings:
                PageSettings::draw(state, viewport, toasts);
                break;
            default:
                PageCharacters::draw(state, characters, viewport, &player);
                break;
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // 4. Draw Interactive Splitters (Draggable Side Panels)
    ImDrawList* fgDraw = ImGui::GetForegroundDrawList(vp);
    ImGuiIO& io = ImGui::GetIO();

    // Left Sidebar Splitter Handle
    float leftSplitterX = vp->Pos.x + state.sideWidth;
    ImGui::SetNextWindowPos(ImVec2(leftSplitterX - 3.0f, panelY));
    ImGui::SetNextWindowSize(ImVec2(6.0f, panelH));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags splitterFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                     ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##LeftSplitterWindow", nullptr, splitterFlags)) {
        ImGui::InvisibleButton("##LeftSplitterBtn", ImVec2(6.0f, panelH));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        if (hovered || active) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (active) {
            state.sideWidth += io.MouseDelta.x;
            state.sideWidth = std::clamp(state.sideWidth, 120.0f, 320.0f);
        }
        ImU32 col = active ? ImGui::GetColorU32(UIStyle::accent) :
                    (hovered ? IM_COL32(100, 200, 120, 220) : IM_COL32(40, 44, 52, 160));
        fgDraw->AddLine(ImVec2(leftSplitterX, panelY), ImVec2(leftSplitterX, panelY + panelH), col, (hovered || active) ? 2.0f : 1.0f);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Right Workspace Panel Splitter Handle
    float rightSplitterX = panelX;
    ImGui::SetNextWindowPos(ImVec2(rightSplitterX - 3.0f, panelY));
    ImGui::SetNextWindowSize(ImVec2(6.0f, panelH));
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("##RightSplitterWindow", nullptr, splitterFlags)) {
        ImGui::InvisibleButton("##RightSplitterBtn", ImVec2(6.0f, panelH));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        if (hovered || active) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (active) {
            state.panelWidth -= io.MouseDelta.x;
            state.panelWidth = std::clamp(state.panelWidth, 260.0f, 640.0f);
        }
        ImU32 col = active ? ImGui::GetColorU32(UIStyle::accent) :
                    (hovered ? IM_COL32(100, 200, 120, 220) : IM_COL32(40, 44, 52, 160));
        fgDraw->AddLine(ImVec2(rightSplitterX, panelY), ImVec2(rightSplitterX, panelY + panelH), col, (hovered || active) ? 2.0f : 1.0f);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // 5. Draw Viewport Floating Overlays (Toolbar & HUDs)
    float viewportX = vp->Pos.x + state.sideWidth;

    // 5a. Top Floating Viewport Toolbar
    {
        float maxToolW = std::max(200.0f, (rightSplitterX - viewportX) - 180.0f);
        float toolW = std::min(570.0f, maxToolW);
        float toolH = 36.0f;
        float toolX = viewportX + 14.0f;
        float toolY = vp->Pos.y + UIStyle::topH + 12.0f;

        ImGui::SetNextWindowPos(ImVec2(toolX, toolY));
        ImGui::SetNextWindowSize(ImVec2(toolW, toolH));
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags toolFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                     ImGuiWindowFlags_NoScrollbar;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.12f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.24f, 0.32f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        if (ImGui::Begin("##ViewportTopToolbar", nullptr, toolFlags)) {
            const float itemH = ImGui::GetFrameHeight();

            // Camera Projection Dropdown
            ImGui::SetNextItemWidth(102);
            static const char* projLabels[] = {"Perspective", "Orthographic"};
            if (ImGui::Combo("##CameraProjCombo", &state.cameraProjection, projLabels, 2)) {
                viewport.setProjection(state.cameraProjection);
            }

            ImGui::SameLine(0, 8);

            // Display Toggles (Grid, Axes, Floor, Skeleton, Wireframe)
            auto drawToggleBtn = [itemH](const char* label, bool& val, bool hasPlus = true) {
                std::string text = (hasPlus && val ? "+ " : (hasPlus ? "+ " : "")) + std::string(label);
                if (val) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.28f, 0.18f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::accent);
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.70f, 0.78f, 1.0f));
                }
                if (ImGui::Button(text.c_str(), ImVec2(0, itemH))) {
                    val = !val;
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine(0, 5);
            };

            drawToggleBtn("Grid", state.showGrid, true);
            drawToggleBtn("Axes", state.showAxes, true);
            drawToggleBtn("Floor", state.showFloor, true);
            drawToggleBtn("Skeleton", state.showSkeleton, true);
            drawToggleBtn("Wireframe", state.showWireframe, false);

            ImGui::SameLine(0, 8);

            // Camera Snapshot & Reset
            if (ImGui::Button(ICON_FA_CAMERA "##Snap1", ImVec2(28, itemH))) {
                std::string snapPath = "screenshot_" + std::to_string(std::time(nullptr)) + ".png";
                TakeScreenshot(snapPath.c_str());
                toasts.push("Snapshot saved: " + snapPath, ToastKind::Success);
            }
            ImGui::SameLine(0, 4);
            if (ImGui::Button(ICON_FA_RESET " Reset", ImVec2(0, itemH))) {
                viewport.reset();
            }
        }
        ImGui::End();

        ImGui::PopStyleVar(5);
        ImGui::PopStyleColor(2);
    }

    // 5b. Top-Right Stats HUD (Skeleton: 30 joints | Vertices: 12,842 | FPS: 60)
    {
        float hudW = 150.0f;
        float hudH = 74.0f;
        float hudX = rightSplitterX - hudW - 14.0f;
        float hudY = vp->Pos.y + UIStyle::topH + 12.0f;

        ImGui::SetNextWindowPos(ImVec2(hudX, hudY));
        ImGui::SetNextWindowSize(ImVec2(hudW, hudH));
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

        if (ImGui::Begin("##ViewportStatsHUD", nullptr, hudFlags)) {
            CharacterAsset* activeChar = characters.activeAsset();
            int vertCount = 12842;
            int jointCount = 30;
            if (activeChar && activeChar->isLoaded()) {
                vertCount = static_cast<int>(activeChar->skinningData().vertices.size());
                jointCount = static_cast<int>(activeChar->bones().size());
            }

            ImGui::TextDisabled("Skeleton:");
            ImGui::SameLine(64.0f);
            ImGui::TextColored(UIStyle::text, "%d joints", jointCount);

            ImGui::TextDisabled("Vertices:");
            ImGui::SameLine(64.0f);
            ImGui::TextColored(UIStyle::text, "%d", vertCount);

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
        float barH = 76.0f;
        float barY = vp->Pos.y + vp->Size.y - UIStyle::statusH - barH - 12.0f;
        float hudW = 230.0f;
        float hudH = 92.0f;
        float hudX = rightSplitterX - hudW - 14.0f;
        float hudY = barY - hudH - 10.0f;

        ImGui::SetNextWindowPos(ImVec2(hudX, hudY));
        ImGui::SetNextWindowSize(ImVec2(hudW, hudH));
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

        if (ImGui::Begin("##ViewportPlaybackHUD", nullptr, hudFlags)) {
            const int curFrame = player.frame();
            const int totalFrames = std::max(1, player.totalFrames());
            const float curTime = player.time();

            ImGui::TextDisabled("Animation:");
            ImGui::SameLine(72.0f);
            std::string animTitle = "A person eating an apple";
            ImGui::TextColored(UIStyle::text, "%s", animTitle.c_str());

            ImGui::TextDisabled("Frame:");
            ImGui::SameLine(72.0f);
            ImGui::TextColored(UIStyle::text, "%03d / %d", curFrame, totalFrames);

            ImGui::TextDisabled("Time:");
            ImGui::SameLine(72.0f);
            ImGui::TextColored(UIStyle::text, "%.2fs / 4.000", curTime);

            ImGui::TextDisabled("Playback:");
            ImGui::SameLine(72.0f);
            ImGui::TextColored(UIStyle::text, "%.1fx", state.playbackSpeed);
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }

    // 6. Draw Bottom Timeline Bar
    TimelineBar::draw(state, player, panelW);

    // 6. Draw Bottom Status Bar
    StatusBar::draw(state, viewport);

    // 7. Draw Toast Notifications Floating Overlay
    toasts.draw();
}

} // namespace studio
