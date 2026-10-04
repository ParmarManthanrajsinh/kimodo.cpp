#include "ui/pages/PageSetup.h"

#include <cstdio>
#include "imgui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "utils/AppPaths.h"
#include "utils/FileDialog.h"

namespace studio
{

namespace
{

std::string FormatBytes(uint64_t bytes)
{
    if (bytes >= (1024ULL * 1024ULL * 1024ULL))
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
        return buf;
    }
    if (bytes >= (1024ULL * 1024ULL))
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        return buf;
    }
    if (bytes >= 1024ULL)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
        return buf;
    }
    return std::to_string(bytes) + " B";
}

std::string FormatEta(int total_seconds)
{
    if (total_seconds <= 0)
    {
        return "Calculating...";
    }
    int mins = total_seconds / 60;
    int secs = total_seconds % 60;
    if (mins >= 60)
    {
        int hrs = mins / 60;
        mins = mins % 60;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "~%dh %02dm", hrs, mins);
        return buf;
    }
    if (mins > 0)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "~%dm %02ds", mins, secs);
        return buf;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "~%ds", secs);
    return buf;
}

} // namespace

void PageSetup::Draw(AppState& state, SetupManager& setup, ModelManager& models, Toasts& toasts)
{
    const auto& checklist = setup.GetChecklist();
    SetupState cur_state = setup.GetState();

    // Top Header
    ImGui::TextColored(UIStyle::accent, "%s Kimodo Studio Setup", icons::kKimodo);
    ImGui::TextDisabled("AI Character Motion Generation & Animation Workstation");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Responsive container centered horizontally
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float max_card_w = 760.0f;
    const float card_w = (avail_w > max_card_w) ? max_card_w : (avail_w - 16.0f);
    const float side_indent = (avail_w > card_w) ? ((avail_w - card_w) * 0.5f) : 0.0f;

    if (side_indent > 0.0f)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + side_indent);
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.14f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.24f, 0.32f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);

    ImGui::BeginChild("##WizardCard", ImVec2(card_w, 0), true);
    {
        // =============================================================
        // 1. WELCOME SCREEN
        // =============================================================
        if (cur_state == SetupState::Welcome)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::accent, "%s Welcome to Kimodo Studio", icons::kKimodo);
            ImGui::TextColored(ImVec4(0.9f, 0.92f, 0.96f, 1.0f), "AI Character Motion & Animation Workstation");
            ImGui::Spacing();
            ImGui::TextWrapped("Create AI-powered character animation from text prompts directly on your local GPU.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(UIStyle::text, "Key Features:");
            ImGui::Spacing();
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("Automatic AI model installation (SOMA RP v1.1 & LLM2Vec text encoder)");

            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("Hardware GPU acceleration via native Vulkan & GGML runtime");

            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("Blender & generic humanoid 3D workflow");

            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("Direct BVH export for Unreal Engine IK Rig retargeting");

            ImGui::Spacing();
            ImGui::Spacing();

            // Primary Action
            ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            if (ImGui::Button(ICON_FA_PLAY "  Get Started", ImVec2(240, 44)))
            {
                setup.StartFirstRun(models, state);
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextDisabled("Already have everything installed?");
            if (ImGui::Button(ICON_FA_FOLDER "  Advanced / Existing Installation", ImVec2(260, 32)))
            {
                state.screen = Screen::settings;
                toasts.Push("Opening Settings -> AI & Models", ToastKind::Info);
            }
        }

        // =============================================================
        // 2. CHECKING SYSTEM (Transient)
        // =============================================================
        else if (cur_state == SetupState::CheckingSystem)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::yellow, "%s Checking system components and local models...", icons::kSpinner);
            ImGui::TextDisabled("Kimodo Studio is verifying existing neural weights and runtime libraries.");
            ImGui::Spacing();
            ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()), ImVec2(-1, 20), "Scanning...");
        }

        // =============================================================
        // 3. NEEDS HUGGING FACE LOGIN
        // =============================================================
        else if (cur_state == SetupState::NeedsHuggingFaceLogin)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::accent, "%s Sign in with Hugging Face", icons::kUser);
            ImGui::TextDisabled("Authenticate Kimodo Studio to access and download required AI models.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextWrapped("A browser window will open. Authorize Kimodo Studio and return here.");
            ImGui::Spacing();

            if (!setup.GetErrorMessage().empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::yellow);
                ImGui::TextWrapped("%s %s", icons::kWarn, setup.GetErrorMessage().c_str());
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }

            ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            if (ImGui::Button(ICON_FA_USER "  Sign in with Hugging Face", ImVec2(260, 44)))
            {
                std::string err;
                if (!setup.StartDeviceLogin(err))
                {
                    toasts.Push("Failed to initiate login: " + err, ToastKind::Error);
                }
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextDisabled("No manual token pasting required. Your login is securely stored in Windows Credential Manager.");
        }

        // =============================================================
        // 4. AUTHENTICATING (Waiting for Device Code Approval)
        // =============================================================
        else if (cur_state == SetupState::Authenticating)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::yellow, "%s Connecting to Hugging Face...", icons::kSpinner);
            ImGui::TextDisabled("Approve Kimodo Studio in your opened browser window.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const auto& auth = setup.GetDeviceAuth();

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.22f, 0.8f));
            ImGui::BeginChild("##DeviceCodeBox", ImVec2(0, 110), true);
            {
                ImGui::Text("Confirmation Code:");
                ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::accent);
                ImGui::SetWindowFontScale(1.4f);
                ImGui::Text("%s", auth.user_code.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Click to copy code: %s", auth.user_code.c_str());
                }
                if (ImGui::IsItemClicked())
                {
                    ImGui::SetClipboardText(auth.user_code.c_str());
                    toasts.Push("Copied code to clipboard: " + auth.user_code, ToastKind::Success);
                }

                ImGui::SameLine(0, 16);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.28f, 0.36f, 1.0f));
                if (ImGui::Button(ICON_FA_COPY "  Copy Code", ImVec2(120, 28)))
                {
                    ImGui::SetClipboardText(auth.user_code.c_str());
                    toasts.Push("Copied code to clipboard: " + auth.user_code, ToastKind::Success);
                }
                ImGui::PopStyleColor(2);

                ImGui::Spacing();
                ImGui::TextDisabled("Verify this code matches the code displayed on huggingface.co.");
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_LINK "  Open Authorization Page Again", ImVec2(270, 36)))
            {
                HuggingFaceClient::OpenBrowser(auth.verification_uri_complete);
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_CLOSE "  Cancel", ImVec2(120, 36)))
            {
                setup.CancelDeviceLogin();
            }
        }

        // =============================================================
        // 5. CHECKING MODEL ACCESS
        // =============================================================
        else if (cur_state == SetupState::CheckingModelAccess)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::yellow, "%s Verifying model access...", icons::kSpinner);
            ImGui::TextDisabled("Checking permissions for required gated repositories on Hugging Face.");
            ImGui::Spacing();
            ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()), ImVec2(-1, 20), "Verifying permissions...");
        }

        // =============================================================
        // 6. NEEDS MODEL ACCESS
        // =============================================================
        else if (cur_state == SetupState::NeedsModelAccess)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::yellow, "%s Additional Access Required", icons::kWarn);
            ImGui::TextDisabled("Kimodo Studio needs access to the following gated models on Hugging Face:");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const auto& missing = setup.GetMissingAccessModels();
            for (const auto& item : missing)
            {
                ImGui::BulletText("%s (%s)", item.first.c_str(), item.second.c_str());
                ImGui::SameLine();
                std::string btn_label = "Request Access##" + item.second;
                if (ImGui::SmallButton(btn_label.c_str()))
                {
                    HuggingFaceClient::OpenBrowser("https://huggingface.co/" + item.second);
                }
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Text("After requesting access on Hugging Face:");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            if (ImGui::Button(ICON_FA_REPEAT "  Check Access Again", ImVec2(220, 40)))
            {
                setup.CheckModelAccessAll(models);
            }
            ImGui::PopStyleColor(2);
        }

        // =============================================================
        // 7. DOWNLOADING (Automatic Component Installation)
        // =============================================================
        else if (cur_state == SetupState::Downloading)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::accent, "%s Installing Kimodo AI", icons::kDownload);
            ImGui::TextDisabled("Automatically downloading and preparing required AI components.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Component Checklist
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.06f, 0.09f, 0.8f));
            ImGui::BeginChild("##ComponentChecklist", ImVec2(0, 116), true);
            {
                // SOMA RP
                if (checklist.motion_model_ready)
                {
                    ImGui::TextColored(UIStyle::green, "%s SOMA RP v1.1                 Installed", icons::kCheck);
                }
                else
                {
                    ImGui::TextColored(UIStyle::yellow, "%s SOMA RP v1.1                 Processing...", icons::kDownload);
                }

                // Text Encoder Bundle
                if (checklist.text_encoder_ready)
                {
                    ImGui::TextColored(UIStyle::green, "%s LLM2Vec Text Encoder         Installed", icons::kCheck);
                }
                else
                {
                    ImGui::TextColored(UIStyle::yellow, "%s LLM2Vec Text Encoder (35 files) Downloading...", icons::kDownload);
                }

                // Character Assets
                if (checklist.character_assets_ready)
                {
                    ImGui::TextColored(UIStyle::green, "%s Character Assets             Ready", icons::kCheck);
                }
                else
                {
                    ImGui::TextColored(UIStyle::text_muted, "%s Character Assets             Waiting", icons::kCheck);
                }

                // Runtime
                if (checklist.runtime_ready)
                {
                    ImGui::TextColored(UIStyle::green, "%s AI Tensor Runtime            Ready", icons::kCheck);
                }
                else
                {
                    ImGui::TextColored(UIStyle::text_muted, "%s AI Tensor Runtime            Checking", icons::kCheck);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Active Download Progress Card
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.13f, 0.18f, 0.9f));
            ImGui::BeginChild("##ActiveProgressCard", ImVec2(0, 110), true);
            {
                ImGui::TextColored(UIStyle::accent, "%s %s", icons::kSpinner, setup.GetCurrentTask().c_str());

                // Bytes & Progress
                uint64_t done = setup.GetBytesDone();
                uint64_t total = setup.GetBytesTotal();
                float prog = setup.GetProgress();

                char prog_buf[128];
                if (total > 0)
                {
                    std::snprintf(prog_buf, sizeof(prog_buf), "%s / %s (%.1f%%)",
                                  FormatBytes(done).c_str(), FormatBytes(total).c_str(), prog * 100.0f);
                }
                else
                {
                    std::snprintf(prog_buf, sizeof(prog_buf), "%s", FormatBytes(done).c_str());
                }

                ImGui::ProgressBar(prog, ImVec2(-1, 22), prog_buf);

                // Speed and ETA
                float speed = setup.GetDownloadSpeedMbps();
                int eta = setup.GetEtaSeconds();
                ImGui::TextDisabled("Download Speed: %.1f MB/s  |  Estimated Time: %s",
                                    speed, FormatEta(eta).c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_CLOSE "  Cancel Installation", ImVec2(180, 32)))
            {
                setup.CancelDownload(models);
                toasts.Push("Installation cancelled", ToastKind::Warning);
            }
        }

        // =============================================================
        // 8. VERIFYING
        // =============================================================
        else if (cur_state == SetupState::Verifying)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::yellow, "%s Verifying Installed Files", icons::kSpinner);
            ImGui::TextDisabled("Validating SHA-256 cryptographic integrity of all downloaded neural weights.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("%s", setup.GetCurrentTask().c_str());
            ImGui::ProgressBar(setup.GetProgress(), ImVec2(-1, 24));
        }

        // =============================================================
        // 9. INSTALLING
        // =============================================================
        else if (cur_state == SetupState::Installing)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::accent, "%s Finalizing Installation", icons::kCheck);
            ImGui::TextDisabled("Activating models and configuring the neural motion runtime.");
            ImGui::Spacing();
            ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()), ImVec2(-1, 20), "Registering neural models...");
        }

        // =============================================================
        // 10. CHECKING RUNTIME
        // =============================================================
        else if (cur_state == SetupState::CheckingRuntime)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::accent, "%s Validating Hardware & Runtime", icons::kCheckCircle);
            ImGui::TextDisabled("Ensuring GPU hardware acceleration and tensor engines are ready.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(checklist.runtime_ready ? UIStyle::green : UIStyle::red,
                               "%s AI Runtime: %s", checklist.runtime_ready ? icons::kCheck : icons::kWarn,
                               checklist.runtime_ready ? "Configured & Ready" : "Missing Libraries");

            ImGui::TextColored(checklist.gpu_ready ? UIStyle::green : UIStyle::yellow,
                               "%s GPU Acceleration: %s", checklist.gpu_ready ? icons::kCheck : icons::kWarn,
                               checklist.gpu_name.c_str());

            ImGui::TextColored(checklist.memory_ready ? UIStyle::green : UIStyle::yellow,
                               "%s Required Memory: %s", checklist.memory_ready ? icons::kCheck : icons::kWarn,
                               checklist.memory_details.c_str());

            ImGui::TextColored(checklist.character_assets_ready ? UIStyle::green : UIStyle::red,
                               "%s Character Assets: %s", checklist.character_assets_ready ? icons::kCheck : icons::kWarn,
                               checklist.character_assets_ready ? "CesiumMan Rig & PBR Ready" : "Missing Assets");
        }

        // =============================================================
        // 11. READY SCREEN
        // =============================================================
        else if (cur_state == SetupState::Ready)
        {
            ImGui::Spacing();
            ImGui::TextColored(UIStyle::green, "%s You're ready.", icons::kCheck);
            ImGui::TextColored(ImVec4(0.9f, 0.92f, 0.96f, 1.0f), "Kimodo Studio is ready to create AI-powered animation.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(UIStyle::green, "%s AI models installed", icons::kCheck);
            ImGui::TextColored(UIStyle::green, "%s Runtime configured", icons::kCheck);
            ImGui::TextColored(UIStyle::green, "%s GPU acceleration available (%s)", icons::kCheck, checklist.gpu_name.c_str());
            ImGui::TextColored(UIStyle::green, "%s Character assets ready", icons::kCheck);

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
            if (ImGui::Button(ICON_FA_PLAY "  Start Creating Motion", ImVec2(280, 48)))
            {
                state.screen = Screen::Generate;
                state.last_tool_screen = Screen::Generate;
                toasts.Push("Ready! Start creating humanoid motion clips.", ToastKind::Success);
            }
            ImGui::PopStyleColor(2);
        }

        // =============================================================
        // 12. ERROR SCREEN (Clean Verification & Recovery UX)
        // =============================================================
        else if (cur_state == SetupState::Error)
        {
            ImGui::Spacing();
            const auto& vf = setup.GetVerificationFailureDetails();

            if (vf.has_failure)
            {
                ImGui::TextColored(UIStyle::red, "%s Download verification failed", icons::kWarn);
                ImGui::TextWrapped("The downloaded file could not be verified and was discarded.");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button(ICON_FA_REPEAT "  Try Again", ImVec2(160, 36)))
                {
                    setup.Retry(models, state);
                }
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_FOLDER "  Choose Another File", ImVec2(210, 36)))
                {
                    std::string picked;
                    if (FileDialog::OpenFile("gguf", AppPaths::DefaultModelsDir().string().c_str(), picked))
                    {
                        models.ImportAsync(picked, "soma-rp-v1.1");
                        toasts.Push("Importing local file...", ToastKind::Info);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Return to Setup", ImVec2(150, 36)))
                {
                    setup.ReturnToSetup();
                }

                ImGui::Spacing();
                // Collapsed Details section
                if (ImGui::CollapsingHeader("Details"))
                {
                    ImGui::Indent();
                    if (!vf.filename.empty()) ImGui::TextDisabled("Filename: %s", vf.filename.c_str());
                    if (!vf.source.empty()) ImGui::TextDisabled("Source: %s", vf.source.c_str());
                    if (!vf.expected_sha256.empty()) ImGui::TextDisabled("Expected SHA-256: %s", vf.expected_sha256.c_str());
                    if (!vf.received_sha256.empty()) ImGui::TextDisabled("Received SHA-256: %s", vf.received_sha256.c_str());
                    if (!vf.error_code.empty()) ImGui::TextDisabled("Error Message: %s", vf.error_code.c_str());
                    ImGui::Unindent();
                }
            }
            else
            {
                ImGui::TextColored(UIStyle::red, "%s Setup Incomplete", icons::kWarn);
                ImGui::TextWrapped("%s", setup.GetErrorMessage().c_str());
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button(ICON_FA_REPEAT "  Try Again", ImVec2(160, 36)))
                {
                    setup.Retry(models, state);
                }
                ImGui::SameLine();
                if (ImGui::Button("Return to Setup", ImVec2(150, 36)))
                {
                    setup.ReturnToSetup();
                }
            }
        }

        // =============================================================
        // ADVANCED DETAILS (Collapsed by default for developers)
        // =============================================================
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Advanced Installation Details"))
        {
            ImGui::Indent();
            ImGui::TextDisabled("Models Directory: %s", AppPaths::DefaultModelsDir().string().c_str());
            ImGui::TextDisabled("Motion Model File: %s", checklist.motion_model_path.c_str());
            ImGui::TextDisabled("Text Encoder Directory: %s", checklist.text_encoder_path.c_str());
            ImGui::TextDisabled("App Data Directory: %s", AppPaths::AppDataDir().string().c_str());
            ImGui::TextDisabled("Config File: %s", AppPaths::ResolveConfig("models.json").string().c_str());
            ImGui::TextDisabled("Tensor Runtime: %s", checklist.runtime_details.c_str());
            ImGui::TextDisabled("GPU Backend: %s (%s)", checklist.backend_name.c_str(), checklist.vram_info.c_str());
            ImGui::TextDisabled("Runtime Version: %s", checklist.runtime_version.c_str());
            ImGui::Unindent();
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace studio
