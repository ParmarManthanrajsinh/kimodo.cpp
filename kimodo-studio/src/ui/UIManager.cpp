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
    float panelW = UIStyle::panelW;
    float panelX = vp->Pos.x + vp->Size.x - panelW;
    float panelY = vp->Pos.y + UIStyle::topH;
    float panelH = vp->Size.y - UIStyle::topH - UIStyle::statusH;

    // Expand panel for character inspector / large pages
    if (state.screen == Screen::Characters || state.screen == Screen::Home) {
        panelW = vp->Size.x - UIStyle::sideW - 320.0f;
        if (panelW < UIStyle::panelW) panelW = UIStyle::panelW;
        panelX = vp->Pos.x + vp->Size.x - panelW;
    }

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
                PageHome::draw(state, library, characters);
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
            case Screen::Characters:
                PageCharacters::draw(state, characters, viewport);
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
                PageGenerate::draw(state, engine, models, toasts);
                break;
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // 4. Draw Bottom Timeline Bar
    TimelineBar::draw(state, player, panelW);

    // 5. Draw Bottom Status Bar
    StatusBar::draw(state, viewport);

    // 6. Draw Toast Notifications Floating Overlay
    toasts.draw();
}

} // namespace studio
