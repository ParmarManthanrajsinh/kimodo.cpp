#include "app/SetupManager.h"

#include <filesystem>
#include "huggingface/HFAuthenticator.h"
#include "utils/AppPaths.h"

namespace studio
{

void SetupManager::Init(ModelManager& models, AppState& app_state)
{
    // Check if token exists in Windows Credential Manager
    std::string token;
    if (HFAuthenticator::LoadToken(token))
    {
        checklist.hf_connected = true;
        if (app_state.hfUser.empty())
        {
            std::string err;
            std::string user = HFAuthenticator::Validate(token, err);
            if (!user.empty())
            {
                app_state.hfUser = user;
                checklist.hf_username = user;
            }
        }
        else
        {
            checklist.hf_username = app_state.hfUser;
        }
    }

    CheckStatus(models, app_state);
}

void SetupManager::CheckStatus(ModelManager& models, AppState& app_state)
{
    models.Rescan();

    // 1. Motion Model
    ModelEntry soma_entry;
    if (models.find_copy("soma-rp-v1.1", soma_entry) && soma_entry.installed)
    {
        checklist.motion_model_ready = true;
        checklist.motion_model_path = soma_entry.local_path;
    }
    else
    {
        checklist.motion_model_ready = false;
        checklist.motion_model_path.clear();
        // Also check if any installed model can act as motion model
        std::string active_id = models.GetActiveId();
        if (!active_id.empty() && models.find_copy(active_id, soma_entry) && soma_entry.installed)
        {
            checklist.motion_model_ready = true;
            checklist.motion_model_path = soma_entry.local_path;
        }
    }

    // 2. Text Encoder Bundle
    ModelEntry text_entry;
    if (models.find_copy("llm2vec-text-bundle", text_entry) && text_entry.installed)
    {
        checklist.text_encoder_ready = true;
        checklist.text_encoder_path = text_entry.local_path;
    }
    else
    {
        std::filesystem::path bundle = AppPaths::ResolveTextBundle("llm2vec-text-bundle");
        std::error_code ec;
        if (std::filesystem::is_directory(bundle, ec) && !ec)
        {
            checklist.text_encoder_ready = true;
            checklist.text_encoder_path = bundle.string();
        }
        else
        {
            checklist.text_encoder_ready = false;
            checklist.text_encoder_path.clear();
        }
    }

    // 3. Character Assets
    std::filesystem::path char_path = AppPaths::ResolveAsset("assets/characters/CesiumMan.glb");
    std::error_code ec;
    checklist.character_assets_ready = std::filesystem::is_regular_file(char_path, ec) && !ec;

    // 4. Runtime & GPU Acceleration
    checklist.runtime_ready = !app_state.gpu_name.empty();
    checklist.gpu_name = app_state.gpu_name;

    // 5. Hugging Face
    std::string token;
    if (HFAuthenticator::LoadToken(token))
    {
        checklist.hf_connected = true;
        if (!app_state.hfUser.empty())
        {
            checklist.hf_username = app_state.hfUser;
        }
    }
    else
    {
        checklist.hf_connected = false;
        checklist.hf_username.clear();
    }

    // Determine state
    if (offline_override)
    {
        current_state = SetupState::Ready;
        current_task = "Offline Mode";
    }
    else if (checklist.motion_model_ready && checklist.text_encoder_ready && checklist.character_assets_ready &&
             checklist.runtime_ready)
    {
        current_state = SetupState::Ready;
        current_task = "Ready";
    }
    else if (current_state != SetupState::Downloading && current_state != SetupState::Verifying &&
             current_state != SetupState::Error)
    {
        current_state = SetupState::NeedsLogin;
        current_task = "Awaiting Setup";
    }
}

void SetupManager::Update(ModelManager& models, AppState& app_state)
{
    if (current_state == SetupState::Downloading)
    {
        if (models.IsBusy())
        {
            progress = models.GetTaskProgress();
            current_task = models.GetTaskLabel();
        }
        else
        {
            std::string label = models.GetTaskLabel();
            std::string lower = label;
            for (char& c : lower)
            {
                c = static_cast<char>(std::tolower(c));
            }

            if (lower.find("failed") != std::string::npos || lower.find("mismatch") != std::string::npos ||
                lower.find("error") != std::string::npos || lower.find("cancelled") != std::string::npos)
            {
                current_state = SetupState::Error;
                error_message = label;
                current_task = "Download failed";
            }
            else
            {
                models.Rescan();
                auto missing = models.GetMissingRequired();
                if (missing.empty())
                {
                    current_state = SetupState::Verifying;
                    current_task = "Verifying installation...";
                    VerifyAll(models);
                }
                else
                {
                    // Start next missing download
                    active_download_id = missing.front().id;
                    current_task = "Downloading " + missing.front().name + "...";
                    models.DownloadAsync(active_download_id);
                }
            }
        }
    }
    else if (current_state == SetupState::Verifying)
    {
        if (models.IsBusy())
        {
            progress = models.GetTaskProgress();
            current_task = models.GetTaskLabel();
        }
        else
        {
            std::string label = models.GetTaskLabel();
            std::string lower = label;
            for (char& c : lower)
            {
                c = static_cast<char>(std::tolower(c));
            }

            if (lower.find("mismatch") != std::string::npos || lower.find("failed") != std::string::npos)
            {
                current_state = SetupState::Error;
                error_message = label;
                current_task = "Verification failed";
            }
            else
            {
                CheckStatus(models, app_state);
                if (checklist.motion_model_ready)
                {
                    current_state = SetupState::Ready;
                    current_task = "Setup complete! Ready to generate.";
                }
                else
                {
                    current_state = SetupState::NeedsLogin;
                    current_task = "Awaiting Setup";
                }
            }
        }
    }
}

void SetupManager::StartDownloadMissing(ModelManager& models)
{
    error_message.clear();
    models.Rescan();
    auto missing = models.GetMissingRequired();
    if (missing.empty())
    {
        current_state = SetupState::Ready;
        current_task = "All required assets installed";
        return;
    }

    active_download_id = missing.front().id;
    current_state = SetupState::Downloading;
    current_task = "Downloading " + missing.front().name + "...";
    models.DownloadAsync(active_download_id);
}

void SetupManager::CancelDownload(ModelManager& models)
{
    models.CancelTask();
    current_state = SetupState::NeedsLogin;
    current_task = "Download cancelled";
    error_message = "Download cancelled by user";
}

void SetupManager::VerifyAll(ModelManager& models)
{
    current_state = SetupState::Verifying;
    current_task = "Verifying checksum...";
    models.VerifyAsync("soma-rp-v1.1");
}

bool SetupManager::SignInHF(const std::string& token, std::string& error_out)
{
    if (token.empty())
    {
        error_out = "Token cannot be empty";
        return false;
    }

    std::string err;
    std::string user = HFAuthenticator::Validate(token, err);
    if (user.empty())
    {
        error_out = err.empty() ? "Invalid Hugging Face token" : err;
        return false;
    }

    if (!HFAuthenticator::SaveToken(token, err))
    {
        error_out = "Failed to store credential: " + err;
        return false;
    }

    checklist.hf_connected = true;
    checklist.hf_username = user;
    error_message.clear();
    return true;
}

void SetupManager::SignOutHF(AppState& app_state)
{
    HFAuthenticator::ClearToken();
    checklist.hf_connected = false;
    checklist.hf_username.clear();
    app_state.hfUser.clear();
}

bool SetupManager::CheckHFConnection(std::string& error_out)
{
    std::string token;
    if (!HFAuthenticator::LoadToken(token))
    {
        error_out = "No Hugging Face token saved in credential store.";
        return false;
    }

    std::string user = HFAuthenticator::Validate(token, error_out);
    if (user.empty())
    {
        if (error_out.empty())
        {
            error_out = "Failed to validate token with Hugging Face.";
        }
        return false;
    }

    checklist.hf_connected = true;
    checklist.hf_username = user;
    return true;
}

void SetupManager::Retry(ModelManager& models, AppState& app_state)
{
    error_message.clear();
    CheckStatus(models, app_state);
    if (!IsReady())
    {
        StartDownloadMissing(models);
    }
}

void SetupManager::ContinueOffline()
{
    offline_override = true;
    current_state = SetupState::Ready;
    current_task = "Offline Mode active";
}

std::string SetupManager::GetStatusSummary() const
{
    std::string summary = "AI Runtime:\n";
    summary += checklist.motion_model_ready ? "  [OK] Motion Model (SOMA RP v1.1)\n" : "  [MISSING] Motion Model\n";
    summary += checklist.text_encoder_ready ? "  [OK] Text Encoder (LLM2Vec)\n" : "  [MISSING] Text Encoder\n";
    summary += checklist.character_assets_ready ? "  [OK] Character Assets (CesiumMan)\n"
                                               : "  [MISSING] Character Assets\n";
    summary += checklist.runtime_ready ? ("  [OK] Runtime (" + checklist.gpu_name + ")\n") : "  [WARN] Runtime\n";
    summary += checklist.hf_connected ? ("  [OK] Hugging Face (@" + checklist.hf_username + ")\n")
                                      : "  [--] Hugging Face (Guest / Not Signed In)\n";
    summary += "Status: " + (IsReady() ? std::string("READY - Start generating") : std::string("SETUP INCOMPLETE"));
    return summary;
}

} // namespace studio
