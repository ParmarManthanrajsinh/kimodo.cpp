#include "ui/pages/PageGenerate.h"
#include "app/SetupManager.h"
#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "models/ModelManager.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "utils/AppPaths.h"

namespace studio
{

void PageGenerate::Draw(AppState& state, KimodoEngine& engine, ModelManager& models, SetupManager& setup, Toasts& toasts)
{
    ImGui::TextColored(UIStyle::accent, "%s Motion Generation", icons::kGenerate);
    ImGui::TextDisabled("Generate humanoid motion clips using the SOMA model");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Check active installed model & text bundle completeness
    ModelEntry active_model;
    bool has_model = models.find_copy(models.GetActiveId(), active_model) && active_model.installed;
    bool has_text_bundle = models.IsBundleComplete("llm2vec-text-bundle");
    bool is_setup_ready = setup.IsReady() && has_model && has_text_bundle;

    if (!is_setup_ready)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.14f, 0.08f, 0.95f));
        ImGui::BeginChild("##SetupIncompleteWarning", ImVec2(0, 130), true);
        {
            ImGui::TextColored(UIStyle::yellow, "%s AI model not installed", icons::kWarn);
            ImGui::TextWrapped("Motion generation requires one additional component.");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            if (ImGui::Button(ICON_FA_DOWNLOAD "  Install Now", ImVec2(200, 38)))
            {
                state.screen = Screen::Setup;
                setup.StartFirstRun(models, state);
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    else
    {
        ImGui::TextColored(UIStyle::green, "%s Active Model: %s", icons::kCheck, active_model.name.c_str());
        ImGui::Spacing();
    }

    // Text Prompt Input
    ImGui::Text("Text Prompt:");
    char prompt_buf[512];
    strncpy_s(prompt_buf, sizeof(prompt_buf), state.prompt.c_str(), sizeof(prompt_buf) - 1);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextMultiline("##PromptInput", prompt_buf, sizeof(prompt_buf), ImVec2(-1, 80)))
    {
        state.prompt = prompt_buf;
    }

    // Prompt Presets
    ImGui::Spacing();
    ImGui::TextDisabled("Presets:");
    ImGui::SameLine();
    const char* presets[] = {"A person walks forward.", "A person runs in a circle.",
                             "A person jumps over an obstacle.", "A person waves both hands enthusiastically.",
                             "A person dances happily."};
    for (int i = 0; i < 5; ++i)
    {
        if (i > 0)
            ImGui::SameLine();
        std::string label = "P" + std::to_string(i + 1);
        if (ImGui::SmallButton(label.c_str()))
        {
            state.prompt = presets[i];
        }
        if (ImGui::IsItemHovered())
        {
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

    int seed_int = static_cast<int>(state.seed);
    if (ImGui::InputInt("Seed", &seed_int))
    {
        state.seed = (seed_int >= 0) ? static_cast<unsigned long long>(seed_int) : 0;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_REPEAT " Randomize"))
    {
        state.seed = static_cast<unsigned long long>(std::rand());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Action Button / Status
    EngineStatus est = engine.GetStatus();
    if (est == EngineStatus::Generating)
    {
        ImGui::ProgressBar(engine.GetProgress(), ImVec2(-1, 32));
        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_CLOSE " Cancel Generation", ImVec2(-1, 36)))
        {
            engine.Cancel();
        }
    }
    else
    {
        if (!is_setup_ready)
        {
            ImGui::BeginDisabled();
            ImGui::Button(ICON_FA_GENERATE "  Generate Animation (Setup Required)", ImVec2(-1, 44));
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Please click [Complete Setup Wizard] above to install required AI assets.");
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            if (ImGui::Button(ICON_FA_GENERATE "  Generate Animation", ImVec2(-1, 44)))
            {
                if (state.prompt.empty())
                {
                    toasts.Push("Please enter a text prompt to generate motion.", ToastKind::Warning);
                }
                else
                {
                    state.motion_path = active_model.local_path;
                    // Use the verified bundle directory from ModelManager so the
                    // backend receives the legacy directory, never tokenizer.gguf.
                    ModelEntry text_entry;
                    std::string text_path =
                        AppPaths::ResolveTextBundle("llm2vec-text-bundle").string();
                    if (models.find_copy("llm2vec-text-bundle", text_entry) &&
                        text_entry.installed && !text_entry.local_path.empty())
                    {
                        text_path = text_entry.local_path;
                    }
                    state.text_bundle = text_path;
                    engine.SetPaths(active_model.local_path, text_path);
                    GenerationParams params;
                    params.frames = static_cast<uint32_t>(state.frames);
                    params.steps = static_cast<uint32_t>(state.steps);
                    params.seed = state.seed;
                    engine.RequestGenerate(state.prompt, params);
                }
            }
            ImGui::PopStyleColor(2);
        }
    }
}

} // namespace studio
