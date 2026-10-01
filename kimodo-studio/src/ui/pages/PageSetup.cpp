#include "ui/pages/PageSetup.h"

#include <cstring>
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "utils/AppPaths.h"
#include "utils/FileDialog.h"

namespace studio
{

void PageSetup::Draw(AppState& state, SetupManager& setup, ModelManager& models, Toasts& toasts)
{
    const auto& checklist = setup.GetChecklist();
    SetupState cur_state = setup.GetState();

    // Header Title
    ImGui::TextColored(UIStyle::accent, "%s Welcome to Kimodo Studio", icons::kKimodo);
    ImGui::TextDisabled("AI Character Motion Generation & Animation Workstation");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Onboarding container
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.10f, 0.13f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
    ImGui::BeginChild("##WizardContainer", ImVec2(0, 0), true);
    {
        // -------------------------------------------------------------
        // STEP 1 — Hugging Face Connection
        // -------------------------------------------------------------
        ImGui::TextColored(UIStyle::accent, "Step 1 — Hugging Face Account");
        ImGui::TextDisabled("Authenticate to access gated motion models and text encoder bundles.");
        ImGui::Spacing();

        if (checklist.hf_connected && !checklist.hf_username.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.18f, 0.12f, 0.8f));
            ImGui::BeginChild("##HFConnectedBox", ImVec2(0, 68), true);
            {
                ImGui::TextColored(UIStyle::green, "%s Connected as @%s", icons::kCheck,
                                   checklist.hf_username.c_str());
                ImGui::TextDisabled("Securely stored in Windows Credential Manager.");
                ImGui::Spacing();

                if (ImGui::Button(ICON_FA_REPEAT " Check Connection"))
                {
                    std::string err;
                    if (setup.CheckHFConnection(err))
                    {
                        toasts.Push("Hugging Face connection verified: @" + setup.GetChecklist().hf_username,
                                    ToastKind::Success);
                    }
                    else
                    {
                        toasts.Push("HF check failed: " + err, ToastKind::Error);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_CLOSE " Sign Out"))
                {
                    setup.SignOutHF(state);
                    toasts.Push("Signed out of Hugging Face", ToastKind::Info);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        else
        {
            static char token_buf[256] = "";
            ImGui::Text("User Access Token:");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##HFTokenInput", token_buf, sizeof(token_buf), ImGuiInputTextFlags_Password);

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_USER " Sign In with Hugging Face", ImVec2(220, 32)))
            {
                std::string err;
                if (setup.SignInHF(token_buf, err))
                {
                    state.hfUser = setup.GetChecklist().hf_username;
                    toasts.Push("Authenticated as @" + state.hfUser, ToastKind::Success);
                    // Securely clear token input buffer from RAM
                    std::memset(token_buf, 0, sizeof(token_buf));
                }
                else
                {
                    toasts.Push("Authentication failed: " + err, ToastKind::Error);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Check Connection", ImVec2(150, 32)))
            {
                std::string err;
                if (setup.CheckHFConnection(err))
                {
                    toasts.Push("Connection verified: @" + setup.GetChecklist().hf_username, ToastKind::Success);
                }
                else
                {
                    toasts.Push(err.empty() ? "No active connection" : err, ToastKind::Warning);
                }
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Create a User Access Token with Read permissions at huggingface.co/settings/tokens.");
            ImGui::TextDisabled("Note: Gated models require your HF account to be granted access to the model.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // -------------------------------------------------------------
        // STEP 2 — Required AI Components
        // -------------------------------------------------------------
        ImGui::TextColored(UIStyle::accent, "Step 2 — Required AI Components");
        ImGui::TextDisabled("Kimodo Studio verifies and downloads required neural weights automatically.");
        ImGui::Spacing();

        // Status checklist items
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.07f, 0.09f, 0.9f));
        ImGui::BeginChild("##ChecklistCard", ImVec2(0, 118), true);
        {
            // Motion Model
            if (checklist.motion_model_ready)
            {
                ImGui::TextColored(UIStyle::green, "%s Motion Model: SOMA RP v1.1 GGUF (Installed)", icons::kCheck);
            }
            else
            {
                ImGui::TextColored(UIStyle::yellow, "%s Motion Model: SOMA RP v1.1 GGUF (Missing)", icons::kWarn);
            }

            // Text Encoder
            if (checklist.text_encoder_ready)
            {
                ImGui::TextColored(UIStyle::green, "%s Text Encoder: LLM2Vec Bundle (Installed)", icons::kCheck);
            }
            else
            {
                ImGui::TextColored(UIStyle::yellow, "%s Text Encoder: LLM2Vec Bundle (Missing)", icons::kWarn);
            }

            // Character Assets
            if (checklist.character_assets_ready)
            {
                ImGui::TextColored(UIStyle::green, "%s Character Assets: CesiumMan Rig & PBR (Ready)", icons::kCheck);
            }
            else
            {
                ImGui::TextColored(UIStyle::yellow, "%s Character Assets: Bundled assets (Missing)", icons::kWarn);
            }

            // Runtime & GPU
            if (checklist.runtime_ready)
            {
                ImGui::TextColored(UIStyle::green, "%s GPU Tensor Runtime: %s (Ready)", icons::kCheck,
                                   checklist.gpu_name.c_str());
            }
            else
            {
                ImGui::TextColored(UIStyle::yellow, "%s GPU Tensor Runtime (Checking...)", icons::kWarn);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Download & Progress section
        if (cur_state == SetupState::Downloading)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.22f, 0.95f));
            ImGui::BeginChild("##DownloadProgressBox", ImVec2(0, 80), true);
            {
                ImGui::TextColored(UIStyle::yellow, "%s %s", icons::kSpinner, setup.GetCurrentTask().c_str());
                ImGui::ProgressBar(setup.GetProgress(), ImVec2(-110, 26));
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_CLOSE " Cancel", ImVec2(100, 26)))
                {
                    setup.CancelDownload(models);
                    toasts.Push("Download cancelled", ToastKind::Warning);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        else if (cur_state == SetupState::Verifying)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.16f, 0.22f, 0.95f));
            ImGui::BeginChild("##VerifyingProgressBox", ImVec2(0, 64), true);
            {
                ImGui::TextColored(UIStyle::yellow, "%s Verifying SHA-256 Checksums...", icons::kSpinner);
                ImGui::ProgressBar(setup.GetProgress(), ImVec2(-1, 24));
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        else if (!setup.IsReady())
        {
            ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            if (ImGui::Button(ICON_FA_DOWNLOAD "  Download Required Components", ImVec2(280, 40)))
            {
                setup.StartDownloadMissing(models);
                toasts.Push("Starting automatic download of missing components", ToastKind::Info);
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_FOLDER "  Import Existing Model...", ImVec2(220, 40)))
            {
                std::string picked;
                if (FileDialog::OpenFile("gguf", AppPaths::DefaultModelsDir().string().c_str(), picked))
                {
                    models.ImportAsync(picked, "soma-rp-v1.1");
                    toasts.Push("Importing local model file...", ToastKind::Info);
                }
            }
        }

        // -------------------------------------------------------------
        // OFFLINE / FAILURE RECOVERY SECTION
        // -------------------------------------------------------------
        if (cur_state == SetupState::Error)
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.24f, 0.10f, 0.10f, 0.95f));
            ImGui::BeginChild("##ErrorRecoveryCard", ImVec2(0, 108), true);
            {
                ImGui::TextColored(UIStyle::red, "%s Installation Alert: %s", icons::kWarn,
                                   setup.GetErrorMessage().c_str());
                ImGui::TextWrapped("Internet connection required to download missing AI assets.");
                ImGui::Spacing();

                if (ImGui::Button(ICON_FA_REPEAT " Retry Download", ImVec2(160, 32)))
                {
                    setup.Retry(models, state);
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_FOLDER " Import Existing Model", ImVec2(190, 32)))
                {
                    std::string picked;
                    if (FileDialog::OpenFile("gguf", AppPaths::DefaultModelsDir().string().c_str(), picked))
                    {
                        models.ImportAsync(picked, "soma-rp-v1.1");
                        toasts.Push("Importing local model file...", ToastKind::Info);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Continue Offline", ImVec2(150, 32)))
                {
                    setup.ContinueOffline();
                    toasts.Push("Continuing in offline mode", ToastKind::Info);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // -------------------------------------------------------------
        // STEP 3 / FINAL — Ready & Start Creating
        // -------------------------------------------------------------
        if (setup.IsReady())
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.20f, 0.12f, 0.95f));
            ImGui::BeginChild("##ReadyBox", ImVec2(0, 100), true);
            {
                ImGui::TextColored(UIStyle::green, "%s Status: READY — Start generating", icons::kCheck);
                ImGui::TextDisabled("All required neural weights, runtime DLLs, and 3D character rigs verified.");
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
                if (ImGui::Button(ICON_FA_PLAY "  Start Creating Motion", ImVec2(240, 42)))
                {
                    state.screen = Screen::Generate;
                    state.last_tool_screen = Screen::Generate;
                    toasts.Push("Kimodo Studio is ready! Start generating motion.", ToastKind::Success);
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::TextColored(UIStyle::yellow, "%s Status: Setup in progress", icons::kSpinner);
            ImGui::TextDisabled("Complete the steps above to enable motion generation.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Collapsible Advanced Technical Details
        if (ImGui::CollapsingHeader("Advanced Installation Details"))
        {
            ImGui::Indent();
            ImGui::TextDisabled("Models Directory: %s", AppPaths::DefaultModelsDir().string().c_str());
            ImGui::TextDisabled("Motion Model File: %s", checklist.motion_model_path.c_str());
            ImGui::TextDisabled("Text Encoder Path: %s", checklist.text_encoder_path.c_str());
            ImGui::TextDisabled("App Data Directory: %s", AppPaths::AppDataDir().string().c_str());
            ImGui::TextDisabled("Config File: %s", AppPaths::ResolveConfig("models.json").string().c_str());
            ImGui::Unindent();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace studio
