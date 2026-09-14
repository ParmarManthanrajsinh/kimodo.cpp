#include "ui/UIManager.h"

#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "raylib.h"
#include "rendering/Viewport.h"

#include <cstring>

namespace studio {

static const char* screenLabel(Screen s) {
    switch (s) {
        case Screen::Home: return "Home";
        case Screen::Generate: return "Generate";
        case Screen::Models: return "Models";
        case Screen::Library: return "Library";
        case Screen::Settings: return "Settings";
    }
    return "Home";
}

void UIManager::draw(AppState& state, Viewport& viewport, KimodoEngine& engine) {
    const float topH = 36.0f;
    const float sideW = 180.0f;
    const float statusH = 26.0f;
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    // Top bar
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)sw, topH));
    ImGui::Begin("TopBar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::TextUnformatted("Kimodo Studio");
    ImGui::SameLine(sw - 220.0f);
    ImGui::TextDisabled("GPU");
    ImGui::SameLine();
    ImGui::TextUnformatted("ON");
    ImGui::End();

    // Sidebar
    ImGui::SetNextWindowPos(ImVec2(0, topH));
    ImGui::SetNextWindowSize(ImVec2(sideW, (float)sh - topH - statusH));
    ImGui::Begin("Sidebar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    const Screen items[] = {Screen::Home, Screen::Generate, Screen::Models,
                            Screen::Library, Screen::Settings};
    for (Screen item : items) {
        const bool selected = (state.screen == item);
        if (ImGui::Selectable(screenLabel(item), selected)) {
            state.screen = item;
        }
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Reset camera (R)")) {
        viewport.reset();
    }
    ImGui::End();

    // Side panel content per screen
    ImGui::SetNextWindowPos(ImVec2(sideW, topH));
    ImGui::SetNextWindowSize(ImVec2(300.0f, (float)sh - topH - statusH));
    ImGui::Begin("Panel", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::Text("%s", screenLabel(state.screen));
    ImGui::Separator();
    if (state.screen == Screen::Generate) {
        static char promptBuf[1024];
        static bool promptInit = false;
        if (!promptInit) {
            std::strncpy(promptBuf, state.prompt.c_str(), sizeof(promptBuf) - 1);
            promptBuf[sizeof(promptBuf) - 1] = '\0';
            promptInit = true;
        }
        ImGui::TextWrapped("Model: SOMA RP (local)");
        ImGui::Spacing();
        ImGui::InputTextMultiline("Prompt", promptBuf, sizeof(promptBuf),
                                  ImVec2(-1, 64));
        state.prompt = promptBuf;
        ImGui::InputInt("Frames", &state.frames);
        ImGui::InputInt("Steps", &state.steps);
        if (state.frames < 8) state.frames = 8;
        if (state.frames > 600) state.frames = 600;
        if (state.steps < 1) state.steps = 1;
        if (state.steps > 200) state.steps = 200;

        const bool busy = engine.busy();
        if (busy) {
            ImGui::BeginDisabled();
            ImGui::Button("Generating...");
            ImGui::EndDisabled();
        } else if (ImGui::Button("Generate")) {
            GenerationParams params;
            params.frames = static_cast<uint32_t>(state.frames);
            params.steps = static_cast<uint32_t>(state.steps);
            params.seed = state.seed;
            engine.requestGenerate(state.prompt, params);
        }
        ImGui::Spacing();
        ImGui::TextWrapped("Status: %s", engine.message().c_str());
    } else {
        ImGui::TextWrapped("Viewport renders behind panels. "
                           "Left-drag orbit, middle-drag pan, wheel zoom.");
    }
    ImGui::Spacing();
    ImGui::Text("Camera dist: %.1f", viewport.distance());
    if (ImGui::Button("Frame (F)")) {
        viewport.frame();
    }
    ImGui::End();

    // Status bar
    ImGui::SetNextWindowPos(ImVec2(0, (float)sh - statusH));
    ImGui::SetNextWindowSize(ImVec2((float)sw, statusH));
    ImGui::Begin("Status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::Text("FPS %d | %s | no model (Phase 1)", state.fps,
                state.gpuName.c_str());
    ImGui::End();
}

} // namespace studio
