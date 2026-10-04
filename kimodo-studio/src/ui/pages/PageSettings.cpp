#include "ui/pages/PageSettings.h"
#include <cstring>
#include "app/SettingsManager.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "utils/AppPaths.h"
#include "utils/FileDialog.h"

namespace studio
{

void PageSettings::Draw(AppState& state, Viewport& viewport, SetupManager& setup, ModelManager& models, Toasts& toasts)
{
    ImGui::TextColored(UIStyle::accent, "%s Workstation Settings", icons::kSettings);
    ImGui::TextDisabled("Configure workspace preferences, AI model installation, and GPU options");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto& settings = SettingsManager::GetInstance().GetSettings();
    const auto& checklist = setup.GetChecklist();

    static int active_tab = 0;
    ImGui::PushStyleColor(ImGuiCol_Button, (active_tab == 0) ? UIStyle::accent : UIStyle::panel);
    ImGui::PushStyleColor(ImGuiCol_Text, (active_tab == 0) ? ImVec4(0.05f, 0.05f, 0.08f, 1.0f) : UIStyle::text);
    if (ImGui::Button("General", ImVec2(100, 32))) active_tab = 0;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, (active_tab == 1) ? UIStyle::accent : UIStyle::panel);
    ImGui::PushStyleColor(ImGuiCol_Text, (active_tab == 1) ? ImVec4(0.05f, 0.05f, 0.08f, 1.0f) : UIStyle::text);
    if (ImGui::Button("AI & Models", ImVec2(120, 32))) active_tab = 1;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, (active_tab == 2) ? UIStyle::accent : UIStyle::panel);
    ImGui::PushStyleColor(ImGuiCol_Text, (active_tab == 2) ? ImVec4(0.05f, 0.05f, 0.08f, 1.0f) : UIStyle::text);
    if (ImGui::Button("GPU", ImVec2(80, 32))) active_tab = 2;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, (active_tab == 3) ? UIStyle::accent : UIStyle::panel);
    ImGui::PushStyleColor(ImGuiCol_Text, (active_tab == 3) ? ImVec4(0.05f, 0.05f, 0.08f, 1.0f) : UIStyle::text);
    if (ImGui::Button("Animation", ImVec2(100, 32))) active_tab = 3;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, (active_tab == 4) ? UIStyle::accent : UIStyle::panel);
    ImGui::PushStyleColor(ImGuiCol_Text, (active_tab == 4) ? ImVec4(0.05f, 0.05f, 0.08f, 1.0f) : UIStyle::text);
    if (ImGui::Button("Advanced", ImVec2(100, 32))) active_tab = 4;
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // -------------------------------------------------------------
    // TAB 0: GENERAL
    // -------------------------------------------------------------
    if (active_tab == 0)
    {
        ImGui::TextColored(UIStyle::accent, "Interface Theme");
        ImGui::Spacing();
        if (ImGui::RadioButton("Dark (Default)", settings.theme == "Dark"))
        {
            settings.theme = "Dark";
            Theme::Apply();
            SettingsManager::GetInstance().save();
            toasts.Push("Theme changed to Dark", ToastKind::Info);
        }
        ImGui::SameLine(0, 20);
        if (ImGui::RadioButton("Cyber Neon", settings.theme == "Cyber"))
        {
            settings.theme = "Cyber";
            Theme::Apply();
            SettingsManager::GetInstance().save();
            toasts.Push("Theme changed to Cyber Neon", ToastKind::Info);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(UIStyle::accent, "Application Environment");
        ImGui::TextDisabled("Kimodo Studio Version: 1.0.0");
        ImGui::TextDisabled("Runtime Engine: Kimodo SOMA GGML & Vulkan Workstation");
    }

    // -------------------------------------------------------------
    // TAB 1: AI & MODELS
    // -------------------------------------------------------------
    else if (active_tab == 1)
    {
        // 1. Hugging Face Account
        ImGui::TextColored(UIStyle::accent, "Hugging Face Account");
        if (checklist.hf_connected && !checklist.hf_username.empty())
        {
            ImGui::TextColored(UIStyle::green, "%s Connected as @%s", icons::kCheck, checklist.hf_username.c_str());
            ImGui::TextDisabled("Authenticated via OAuth. Securely stored in Windows Credential Manager.");
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_CLOSE "  Sign Out"))
            {
                setup.SignOutHF(state);
                toasts.Push("Signed out of Hugging Face", ToastKind::Info);
            }
        }
        else
        {
            ImGui::TextColored(UIStyle::yellow, "%s Not signed in", icons::kWarn);
            ImGui::TextDisabled("Sign in to automatically access and download gated neural motion models.");
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_USER "  Sign in with Hugging Face (OAuth)"))
            {
                std::string err;
                if (setup.StartDeviceLogin(err))
                {
                    state.screen = Screen::Setup;
                }
                else
                {
                    toasts.Push("Login error: " + err, ToastKind::Error);
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 2. Installed Models
        ImGui::TextColored(UIStyle::accent, "Installed Neural Models");
        auto entries = models.GetEntries();
        for (const auto& entry : entries)
        {
            if (entry.installed)
            {
                ImGui::BulletText("%s (%s) — %s", entry.name.c_str(), entry.version.c_str(), entry.local_path.c_str());
            }
            else
            {
                ImGui::BulletText("%s — Not Installed", entry.name.c_str());
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 3. Model Directory
        ImGui::TextColored(UIStyle::accent, "Model Directory");
        ImGui::TextDisabled("Storage path: %s", AppPaths::DefaultModelsDir().string().c_str());
        if (ImGui::Button(ICON_FA_FOLDER "  Open Models Directory"))
        {
            HuggingFaceClient::OpenBrowser(AppPaths::DefaultModelsDir().string());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 4. Repair / Reinstall & Import
        ImGui::TextColored(UIStyle::accent, "Maintenance & Import");
        if (ImGui::Button(ICON_FA_REPEAT "  Re-verify Installed Models", ImVec2(220, 36)))
        {
            setup.VerifyAll(models);
            toasts.Push("Verifying checksums of all installed models...", ToastKind::Info);
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FOLDER "  Import Local GGUF Model...", ImVec2(230, 36)))
        {
            std::string picked;
            if (FileDialog::OpenFile("gguf", AppPaths::DefaultModelsDir().string().c_str(), picked))
            {
                models.ImportAsync(picked, "soma-rp-v1.1");
                toasts.Push("Importing local neural model...", ToastKind::Info);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 5. Advanced Authentication (Manual Token Entry strictly moved here)
        if (ImGui::CollapsingHeader("Advanced Authentication"))
        {
            ImGui::Indent();
            ImGui::TextWrapped("Manual User Access Token entry for offline machines, custom proxies, or enterprise environments.");
            ImGui::Spacing();

            static char manual_token[256] = "";
            ImGui::Text("User Access Token:");
            ImGui::SetNextItemWidth(380);
            ImGui::InputText("##ManualHFToken", manual_token, sizeof(manual_token), ImGuiInputTextFlags_Password);

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_CHECK "  Save Token to Credential Manager", ImVec2(280, 32)))
            {
                std::string err;
                if (setup.SignInHF(manual_token, err))
                {
                    state.hfUser = setup.GetChecklist().hf_username;
                    toasts.Push("Token verified and saved for @" + state.hfUser, ToastKind::Success);
                    std::memset(manual_token, 0, sizeof(manual_token));
                }
                else
                {
                    toasts.Push("Authentication failed: " + err, ToastKind::Error);
                }
            }
            ImGui::Unindent();
        }
    }

    // -------------------------------------------------------------
    // TAB 2: GPU
    // -------------------------------------------------------------
    else if (active_tab == 2)
    {
        ImGui::TextColored(UIStyle::accent, "GPU Acceleration & Hardware");
        ImGui::Spacing();

        ImGui::TextColored(checklist.gpu_ready ? UIStyle::green : UIStyle::yellow,
                           "%s GPU Device: %s", checklist.gpu_ready ? icons::kCheck : icons::kWarn,
                           checklist.gpu_name.c_str());

        ImGui::TextDisabled("Acceleration Status: %s", checklist.gpu_status_message.c_str());
        ImGui::TextDisabled("Vulkan Backend: %s", checklist.backend_name.c_str());
        ImGui::TextDisabled("Video Memory: %s", checklist.vram_info.c_str());
        ImGui::TextDisabled("System RAM: %s", checklist.memory_details.c_str());
        ImGui::TextDisabled("Tensor Runtime: %s", checklist.runtime_version.c_str());
    }

    // -------------------------------------------------------------
    // TAB 3: ANIMATION & VIEWPORT
    // -------------------------------------------------------------
    else if (active_tab == 3)
    {
        ImGui::TextColored(UIStyle::accent, "3D Viewport & Rendering Defaults");
        ImGui::Spacing();

        bool grid = viewport.ShowGrid();
        if (ImGui::Checkbox("Show 3D Grid", &grid))
        {
            viewport.SetGrid(grid);
            settings.show_grid = grid;
        }
        bool axes = viewport.ShowAxes();
        if (ImGui::Checkbox("Show Coordinate Axes", &axes))
        {
            viewport.SetAxes(axes);
            settings.show_axes = axes;
        }
        bool floor = viewport.ShowFloor();
        if (ImGui::Checkbox("Show Floor Plane", &floor))
        {
            viewport.SetFloor(floor);
            settings.show_floor = floor;
        }
        bool wire = viewport.ShowWireframe();
        if (ImGui::Checkbox("Wireframe Shading", &wire))
        {
            viewport.SetWireframe(wire);
            settings.show_wireframe = wire;
        }
        bool bones = viewport.ShowBoneNames();
        if (ImGui::Checkbox("3D Bone Names", &bones))
        {
            viewport.SetBoneNames(bones);
            settings.show_bone_names = bones;
        }
    }

    // -------------------------------------------------------------
    // TAB 4: ADVANCED
    // -------------------------------------------------------------
    else if (active_tab == 4)
    {
        ImGui::TextColored(UIStyle::accent, "Storage Locations");
        ImGui::Spacing();
        ImGui::TextDisabled("App Data Directory: %s", AppPaths::AppDataDir().string().c_str());
        ImGui::TextDisabled("Models Directory: %s", AppPaths::DefaultModelsDir().string().c_str());
        ImGui::TextDisabled("Characters Directory: %s", AppPaths::DefaultCharactersDir().string().c_str());
        ImGui::TextDisabled("Animations Directory: %s", AppPaths::DefaultAnimationsDir().string().c_str());
        ImGui::TextDisabled("Configuration: %s", AppPaths::ResolveConfig("models.json").string().c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(ICON_FA_CHECK " Save Preferences", ImVec2(180, 36)))
    {
        SettingsManager::GetInstance().save();
        toasts.Push("Preferences saved successfully", ToastKind::Success);
    }
}

} // namespace studio
