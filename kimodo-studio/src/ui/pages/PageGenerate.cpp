#include "ui/pages/PageGenerate.h"
#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "models/ModelManager.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "utils/AppPaths.h"

namespace studio {

void SPageGenerate::Draw(FAppState& state, FKimodoEngine& engine, FModelManager& models, SToasts& toasts) {
    ImGui::TextColored(FUIStyle::accent, "%s Motion Generation", icons::kGenerate);
    ImGui::TextDisabled("Generate humanoid motion clips using the SOMA model");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Check active installed model
    FModelEntry activeModel;
    bool hasModel = models.findCopy(models.GetActiveId(), activeModel) && activeModel.installed;

    if (!hasModel) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.22f, 0.16f, 0.08f, 0.9f));
        ImGui::BeginChild("##NoModelWarning", ImVec2(0, 110), true);
        {
            ImGui::TextColored(FUIStyle::yellow, "%s No Motion Model Installed", icons::kWarn);
            ImGui::TextWrapped("Diffusion weights are required to synthesize 3D human motion.");
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_CUBE " Open Model Manager to Download or Import", ImVec2(-1, 32))) {
                state.screen = EScreen::Models;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    } else {
        ImGui::TextColored(FUIStyle::green, "%s Active Model: %s", icons::kCheck, activeModel.name.c_str());
        ImGui::Spacing();
    }

    // Text Prompt Input
    ImGui::Text("Text Prompt:");
    char promptBuf[512];
    strncpy_s(promptBuf, sizeof(promptBuf), state.prompt.c_str(), sizeof(promptBuf) - 1);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextMultiline("##PromptInput", promptBuf, sizeof(promptBuf),
                                  ImVec2(-1, 80))) {
        state.prompt = promptBuf;
    }

    // Prompt Presets
    ImGui::Spacing();
    ImGui::TextDisabled("Presets:");
    ImGui::SameLine();
    const char* presets[] = {
        "A person walks forward.",
        "A person runs in a circle.",
        "A person jumps over an obstacle.",
        "A person waves both hands enthusiastically.",
        "A person dances happily."
    };
    for (int i = 0; i < 5; ++i) {
        if (i > 0) ImGui::SameLine();
        std::string label = "P" + std::to_string(i + 1);
        if (ImGui::SmallButton(label.c_str())) {
            state.prompt = presets[i];
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", presets[i]);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Generation Parameters
    ImGui::Text("Parameters:");
    ImGui::SliderInt("Frames", &state.frames, 30, 300, "%d frames");
    ImGui::SliderInt("Diffusion Steps", &state.steps, 10, 100, "%d steps");

    int seedInt = static_cast<int>(state.seed);
    if (ImGui::InputInt("Seed", &seedInt)) {
        state.seed = (seedInt >= 0) ? static_cast<unsigned long long>(seedInt) : 0;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_REPEAT " Randomize")) {
        state.seed = static_cast<unsigned long long>(std::rand());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Action Button / Status
    EEngineStatus est = engine.GetStatus();
    if (est == EEngineStatus::Generating) {
        ImGui::ProgressBar(engine.GetProgress(), ImVec2(-1, 32));
        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_CLOSE " Cancel Generation", ImVec2(-1, 36))) {
            engine.cancel();
        }
    } else {
        if (!hasModel) {
            ImGui::BeginDisabled();
            ImGui::Button(ICON_FA_GENERATE "  Generate Animation (Model Required)", ImVec2(-1, 44));
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Please install a motion model from the Models page first.");
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, FUIStyle::accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            if (ImGui::Button(ICON_FA_GENERATE "  Generate Animation", ImVec2(-1, 44))) {
                if (state.prompt.empty()) {
                    toasts.Push("Please enter a text prompt to generate motion.", EToastKind::Warning);
                } else {
                    state.motionPath = activeModel.localPath;
                    engine.SetPaths(activeModel.localPath, FAppPaths::resolveTextBundle("llm2vec-text-bundle").string());
                    FGenerationParams params;
                    params.frames = static_cast<uint32_t>(state.frames);
                    params.steps = static_cast<uint32_t>(state.steps);
                    params.seed = state.seed;
                    engine.requestGenerate(state.prompt, params);
                }
            }
            ImGui::PopStyleColor(2);
        }
    }
}

} // namespace studio
