#include "app/SetupManager.h"

#include <chrono>
#include <filesystem>

#include "huggingface/HFAuthenticator.h"
#include "huggingface/HuggingFaceClient.h"
#include "utils/AppPaths.h"
#include "utils/RuntimeValidator.h"

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

    // Initial state: If everything is already installed and verified, go to Ready.
    // Otherwise start on the clean first-run Welcome screen.
    if (checklist.motion_model_ready && checklist.text_encoder_ready &&
        checklist.character_assets_ready && checklist.runtime_ready)
    {
        current_state = SetupState::Ready;
        current_task = "Kimodo Studio is ready.";
    }
    else
    {
        current_state = SetupState::Welcome;
        current_task = "Welcome to Kimodo Studio";
    }
}

void SetupManager::StartFirstRun(ModelManager& models, AppState& app_state)
{
    current_state = SetupState::CheckingSystem;
    current_task = "Checking system components and models...";
    CheckStatus(models, app_state);

    if (checklist.motion_model_ready && checklist.text_encoder_ready &&
        checklist.character_assets_ready && checklist.runtime_ready)
    {
        current_state = SetupState::CheckingRuntime;
        return;
    }

    // If HF is already connected, check model access; otherwise prompt login
    if (checklist.hf_connected)
    {
        CheckModelAccessAll(models);
    }
    else
    {
        current_state = SetupState::NeedsHuggingFaceLogin;
        current_task = "Hugging Face Sign-in Required";
    }
}

void SetupManager::ReturnToSetup()
{
    error_message.clear();
    ClearVerificationFailure();
    ResetAutoRestart();
    current_state = SetupState::Welcome;
    current_task = "Welcome to Kimodo Studio";
}

bool SetupManager::StartDeviceLogin(std::string& error_out)
{
    error_message.clear();
    device_auth = {};
    if (!HuggingFaceClient::RequestDeviceCode(device_auth, error_out))
    {
        error_message = error_out;
        current_state = SetupState::Error;
        return false;
    }

    device_poll_interval = device_auth.interval > 0 ? device_auth.interval : 5;
    last_device_poll = std::chrono::steady_clock::now();
    current_state = SetupState::Authenticating;
    current_task = "Waiting for Hugging Face authorization in browser...";

    // Open user's default browser with pre-filled code
    HuggingFaceClient::OpenBrowser(device_auth.verification_uri_complete);
    return true;
}

void SetupManager::CancelDeviceLogin()
{
    current_state = SetupState::NeedsHuggingFaceLogin;
    current_task = "Sign in cancelled";
}

void SetupManager::CheckModelAccessAll(ModelManager& models)
{
    current_state = SetupState::CheckingModelAccess;
    current_task = "Verifying repository access on Hugging Face...";
    checklist.missing_access_models.clear();

    std::string token;
    HFAuthenticator::LoadToken(token);

    // Validate access to EVERY required repository, whether currently
    // public or gated. A 401/403/404 must be reported, never bypassed.
    auto entries = models.GetEntries();
    for (const auto& m : entries)
    {
        if (m.required && !m.repo.empty())
        {
            std::string err;
            if (!HuggingFaceClient::CheckModelAccess(m.repo, token, err))
            {
                checklist.missing_access_models.push_back({m.name, m.repo});
            }
        }
    }

    if (!checklist.missing_access_models.empty())
    {
        current_state = SetupState::NeedsModelAccess;
        current_task = "Additional access required for gated models";
    }
    else
    {
        // Access granted for all required components! Automatically download missing components.
        StartDownloadMissing(models);
    }
}

void SetupManager::CheckStatus(ModelManager& models, AppState& app_state)
{
    checklist.missing_components.clear();
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
        std::string active_id = models.GetActiveId();
        if (!active_id.empty() && models.find_copy(active_id, soma_entry) && soma_entry.installed)
        {
            checklist.motion_model_ready = true;
            checklist.motion_model_path = soma_entry.local_path;
        }
        else
        {
            checklist.missing_components.push_back("Motion Model (SOMA RP v1.1)");
        }
    }

    // 2. Text Encoder Bundle (complete 35-file check)
    ModelEntry text_entry;
    if (models.find_copy("llm2vec-text-bundle", text_entry) && text_entry.installed)
    {
        checklist.text_encoder_ready = true;
        checklist.text_encoder_path = text_entry.local_path;
    }
    else
    {
        checklist.text_encoder_ready = false;
        checklist.text_encoder_path.clear();
        checklist.missing_components.push_back("Text Encoder Bundle (LLM2Vec Llama 3 8B)");
    }

    // 3. Character Assets
    std::filesystem::path char_path = AppPaths::ResolveAsset("assets/characters/CesiumMan.glb");
    std::error_code ec;
    checklist.character_assets_ready = std::filesystem::is_regular_file(char_path, ec) && !ec;
    if (!checklist.character_assets_ready)
    {
        checklist.missing_components.push_back("Character Assets (CesiumMan.glb)");
    }

    // 4. Real Runtime & GPU Validation Layer
    RuntimeValidation rv = RuntimeValidator::Validate();
    checklist.runtime_ready = rv.runtime_dlls_valid;
    checklist.gpu_ready = rv.gpu_accelerated;
    checklist.memory_ready = rv.memory_ready;
    checklist.gpu_name = rv.gpu_name;
    checklist.runtime_details = rv.runtime_details;
    checklist.gpu_status_message = rv.vulkan_status_message;
    checklist.memory_details = rv.memory_details;
    checklist.backend_name = rv.backend_name;
    checklist.vram_info = rv.vram_info;
    checklist.runtime_version = rv.runtime_version;
    app_state.gpu_name = rv.gpu_name;
    if (!checklist.runtime_ready)
    {
        checklist.missing_components.push_back("Runtime DLLs: " + rv.runtime_details);
    }

    // 5. Hugging Face Authentication
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
}

void SetupManager::Update(ModelManager& models, AppState& app_state)
{
    // -------------------------------------------------------------
    // 1. Authenticating: Poll Hugging Face OAuth Device Token
    // -------------------------------------------------------------
    if (current_state == SetupState::Authenticating)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_device_poll).count();

        if (elapsed >= device_poll_interval)
        {
            last_device_poll = now;
            std::string token_out;
            std::string poll_err;
            auto res = HuggingFaceClient::PollDeviceToken(device_auth.device_code, token_out, poll_err);

            if (res == HuggingFaceClient::DeviceAuthPollResult::Success)
            {
                std::string save_err;
                if (HFAuthenticator::SaveToken(token_out, save_err))
                {
                    std::string user = HFAuthenticator::Validate(token_out, save_err);
                    checklist.hf_connected = true;
                    checklist.hf_username = user.empty() ? "HF User" : user;
                    app_state.hfUser = checklist.hf_username;
                    // Securely clear token string from memory
                    token_out.assign(token_out.size(), 0);

                    // Proceed automatically to model access check
                    CheckModelAccessAll(models);
                }
                else
                {
                    current_state = SetupState::Error;
                    error_message = "Failed to securely save Hugging Face credential.";
                }
            }
            else if (res == HuggingFaceClient::DeviceAuthPollResult::SlowDown)
            {
                device_poll_interval += 5;
            }
            else if (res == HuggingFaceClient::DeviceAuthPollResult::Expired)
            {
                current_state = SetupState::NeedsHuggingFaceLogin;
                error_message = "Authorization session expired. Please click Sign In again.";
            }
            else if (res == HuggingFaceClient::DeviceAuthPollResult::Denied)
            {
                current_state = SetupState::NeedsHuggingFaceLogin;
                error_message = "Authorization was denied in the browser.";
            }
            else if (res == HuggingFaceClient::DeviceAuthPollResult::Error)
            {
                current_state = SetupState::Error;
                error_message = poll_err.empty() ? "Authorization error" : poll_err;
            }
        }
    }
    // -------------------------------------------------------------
    // 2. Downloading: Track active progress, speed, ETA, and errors
    // -------------------------------------------------------------
    else if (current_state == SetupState::Downloading)
    {
        if (models.IsBusy())
        {
            progress = models.GetTaskProgress();
            current_task = models.GetTaskLabel();
            bytes_done = models.GetTaskDone();
            bytes_total = models.GetTaskTotal();

            // Calculate download speed and ETA
            auto now = std::chrono::steady_clock::now();
            auto time_delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_speed_time).count();
            if (time_delta_ms >= 500)
            {
                if (bytes_done >= last_bytes)
                {
                    uint64_t diff = bytes_done - last_bytes;
                    double sec = static_cast<double>(time_delta_ms) / 1000.0;
                    float instant_speed = static_cast<float>((diff / (1024.0 * 1024.0)) / sec);
                    download_speed_mbps = (download_speed_mbps == 0.0f)
                        ? instant_speed
                        : (download_speed_mbps * 0.7f + instant_speed * 0.3f);

                    if (download_speed_mbps > 0.05f && bytes_total > bytes_done)
                    {
                        uint64_t remaining_bytes = bytes_total - bytes_done;
                        double remaining_mb = static_cast<double>(remaining_bytes) / (1024.0 * 1024.0);
                        eta_seconds = static_cast<int>(remaining_mb / download_speed_mbps);
                    }
                    else
                    {
                        eta_seconds = 0;
                    }
                }
                last_bytes = bytes_done;
                last_speed_time = now;
            }
        }
        else
        {
            // Branch on the explicit worker outcome, never on label substrings:
            // an unrecognized label previously meant "success", silently
            // auto-restarting failed downloads.
            const TaskResult result = models.GetTaskResult();
            std::string label = models.GetTaskLabel();

            if (result == TaskResult::Failed)
            {
                current_state = SetupState::Error;
                error_message = label;
                current_task = "Download failed";
                ResetAutoRestart();

                // Attach exact verifier details (SHAs + byte counts) when present.
                std::string f, exp, rec, msg;
                uint64_t eb = 0, rb = 0;
                bool known = false;
                if (models.GetLastFailure(f, exp, rec, eb, rb, known, msg))
                {
                    std::string detail = msg.empty() ? label : msg;
                    if (eb > 0)
                    {
                        detail += " [expected " + std::to_string(eb) + " bytes, received " +
                                  (known ? std::to_string(rb) + " bytes" : "unknown") + "]";
                    }
                    SetVerificationFailure(f.empty() ? current_file_name : f, exp, rec,
                                           "Hugging Face Hub", detail);
                }
                else
                {
                    SetVerificationFailure(current_file_name, "", "", "Hugging Face Hub", label);
                }
            }
            else if (result == TaskResult::Unknown)
            {
                current_state = SetupState::Error;
                error_message = "Download task ended without a result: " + label;
                current_task = "Download failed";
                ResetAutoRestart();
                SetVerificationFailure(current_file_name, "", "", "Hugging Face Hub", error_message);
            }
            else
            {
                models.Rescan();
                auto missing = models.GetMissingRequired();
                if (missing.empty())
                {
                    ResetAutoRestart();
                    current_state = SetupState::Verifying;
                    current_task = "Verifying all installed components...";
                    VerifyAll(models);
                }
                else if (!NoteAutoRestart(missing.front().id))
                {
                    current_state = SetupState::Error;
                    error_message = "Automatic download retries exhausted for " +
                                    missing.front().name +
                                    ". Check the network connection and press Try Again.";
                    current_task = "Download failed";
                    SetVerificationFailure(current_file_name, "", "", "Hugging Face Hub", error_message);
                }
                else
                {
                    // Start next missing download in queue
                    active_download_id = missing.front().id;
                    current_file_name = missing.front().name;
                    current_task = "Downloading " + missing.front().name + "...";
                    bytes_done = 0;
                    bytes_total = 0;
                    last_bytes = 0;
                    download_speed_mbps = 0.0f;
                    eta_seconds = 0;
                    last_speed_time = std::chrono::steady_clock::now();
                    models.DownloadAsync(active_download_id);
                }
            }
        }
    }
    // -------------------------------------------------------------
    // 3. Verifying: Validate checksums
    // -------------------------------------------------------------
    else if (current_state == SetupState::Verifying)
    {
        if (models.IsBusy())
        {
            progress = models.GetTaskProgress();
            current_task = models.GetTaskLabel();
        }
        else
        {
            const TaskResult result = models.GetTaskResult();
            std::string label = models.GetTaskLabel();

            if (result != TaskResult::Ok)
            {
                current_state = SetupState::Error;
                error_message = (result == TaskResult::Unknown)
                                    ? ("Verification task ended without a result: " + label)
                                    : label;
                current_task = "Verification failed";
                std::string f, exp, rec, msg;
                uint64_t eb = 0, rb = 0;
                bool known = false;
                if (models.GetLastFailure(f, exp, rec, eb, rb, known, msg))
                {
                    std::string detail = msg.empty() ? label : msg;
                    if (eb > 0)
                    {
                        detail += " [expected " + std::to_string(eb) + " bytes, received " +
                                  (known ? std::to_string(rb) + " bytes" : "unknown") + "]";
                    }
                    SetVerificationFailure(f.empty() ? current_file_name : f, exp, rec,
                                           "Local SHA-256 Verification", detail);
                }
                else
                {
                    SetVerificationFailure(current_file_name, "", "", "Local SHA-256 Verification",
                                           error_message);
                }
                verify_queue.clear();
                verify_current.clear();
            }
            else if (!verify_queue.empty())
            {
                // Continue with the next required asset.
                verify_current = verify_queue.front();
                verify_queue.erase(verify_queue.begin());
                current_file_name = verify_current;
                current_task = "Verifying " + verify_current + "...";
                models.VerifyAsync(verify_current);
            }
            else
            {
                verify_current.clear();
                current_state = SetupState::Installing;
                current_task = "Finalizing component installation...";
            }
        }
    }
    // -------------------------------------------------------------
    // 4. Installing: Atomically mark ready and check runtime
    // -------------------------------------------------------------
    else if (current_state == SetupState::Installing)
    {
        models.Rescan();
        models.Select("soma-rp-v1.1");
        current_state = SetupState::CheckingRuntime;
        current_task = "Checking runtime acceleration...";
    }
    // -------------------------------------------------------------
    // 5. CheckingRuntime: Validate tensor runtime & hardware
    // -------------------------------------------------------------
    else if (current_state == SetupState::CheckingRuntime)
    {
        CheckStatus(models, app_state);
        if (checklist.motion_model_ready && checklist.text_encoder_ready &&
            checklist.character_assets_ready && checklist.runtime_ready)
        {
            current_state = SetupState::Ready;
            current_task = "Kimodo Studio is ready to create AI-powered animation.";
        }
        else
        {
            current_state = SetupState::Error;
            error_message = "Runtime or model validation incomplete.";
            if (!checklist.runtime_ready)
            {
                error_message += " Missing runtime DLLs.";
            }
            if (!checklist.motion_model_ready || !checklist.text_encoder_ready)
            {
                error_message += " Required model weights are incomplete.";
            }
        }
    }
}

void SetupManager::ResetAutoRestart()
{
    last_auto_asset.clear();
    auto_restart_count = 0;
}

bool SetupManager::NoteAutoRestart(std::string_view asset_id)
{
    if (asset_id == last_auto_asset)
    {
        ++auto_restart_count;
    }
    else
    {
        last_auto_asset = std::string(asset_id);
        auto_restart_count = 1;
    }
    return auto_restart_count <= kMaxAutoRestarts;
}

void SetupManager::StartDownloadMissing(ModelManager& models)
{
    error_message.clear();
    ClearVerificationFailure();
    ResetAutoRestart();
    models.Rescan();
    auto missing = models.GetMissingRequired();
    if (missing.empty())
    {
        current_state = SetupState::Verifying;
        current_task = "Verifying all installed components...";
        VerifyAll(models);
        return;
    }

    active_download_id = missing.front().id;
    current_file_name = missing.front().name;
    current_state = SetupState::Downloading;
    current_task = "Downloading " + missing.front().name + "...";
    bytes_done = 0;
    bytes_total = 0;
    last_bytes = 0;
    download_speed_mbps = 0.0f;
    eta_seconds = 0;
    last_speed_time = std::chrono::steady_clock::now();
    models.DownloadAsync(active_download_id);
}

void SetupManager::CancelDownload(ModelManager& models)
{
    models.CancelTask();
    verify_queue.clear();
    verify_current.clear();
    ResetAutoRestart();
    current_state = SetupState::Welcome;
    current_task = "Download cancelled";
    error_message = "Download cancelled by user";
}

void SetupManager::VerifyAll(ModelManager& models)
{
    current_state = SetupState::Verifying;
    current_task = "Verifying SHA-256 checksums...";
    verify_queue.clear();
    verify_current.clear();
    ResetAutoRestart();
    // Queue EVERY required asset. Missing assets fail fast instead of
    // silently verifying only the motion model.
    auto missing = models.GetMissingRequired();
    if (!missing.empty())
    {
        current_state = SetupState::Error;
        error_message = "Verification incomplete: required components are missing:";
        for (const auto& m : missing)
        {
            error_message += " " + m.name + ";";
        }
        return;
    }
    for (const auto& e : models.GetEntries())
    {
        if (e.required && e.installed)
        {
            verify_queue.push_back(e.id);
        }
    }
    if (verify_queue.empty())
    {
        current_state = SetupState::Error;
        error_message = "Verification incomplete: no required components installed.";
        return;
    }
    verify_current = verify_queue.front();
    verify_queue.erase(verify_queue.begin());
    current_file_name = verify_current;
    models.VerifyAsync(verify_current);
}

bool SetupManager::SignInHF(std::string_view token, std::string& error_out)
{
    if (token.empty())
    {
        error_out = "Token cannot be empty";
        return false;
    }

    std::string err;
    std::string user = HFAuthenticator::Validate(std::string(token), err);
    if (user.empty())
    {
        error_out = err.empty() ? "Invalid Hugging Face token" : err;
        return false;
    }

    if (!HFAuthenticator::SaveToken(std::string(token), err))
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

bool SetupManager::VerifyHFAccess(std::string_view repo, std::string& error_out)
{
    std::string token;
    HFAuthenticator::LoadToken(token);
    return HuggingFaceClient::CheckModelAccess(std::string(repo), token, error_out);
}

void SetupManager::Retry(ModelManager& models, AppState& app_state)
{
    error_message.clear();
    ClearVerificationFailure();
    ResetAutoRestart();
    CheckStatus(models, app_state);

    if (checklist.motion_model_ready && checklist.text_encoder_ready &&
        checklist.character_assets_ready && checklist.runtime_ready)
    {
        current_state = SetupState::CheckingRuntime;
        return;
    }

    if (!checklist.hf_connected)
    {
        current_state = SetupState::NeedsHuggingFaceLogin;
    }
    else
    {
        CheckModelAccessAll(models);
    }
}

void SetupManager::ContinueOffline(ModelManager& models, AppState& app_state)
{
    CheckStatus(models, app_state);
    if (checklist.motion_model_ready && checklist.text_encoder_ready && checklist.runtime_ready)
    {
        offline_override = true;
        current_state = SetupState::Ready;
        current_task = "Offline Mode (Local Neural Weights Active)";
        error_message.clear();
    }
    else
    {
        offline_override = false;
        current_state = SetupState::Error;
        error_message = "Offline mode: This installation is incomplete. AI generation is unavailable until the required components are installed.";
    }
}

void SetupManager::ContinueOffline()
{
    if (checklist.motion_model_ready && checklist.text_encoder_ready && checklist.runtime_ready)
    {
        offline_override = true;
        current_state = SetupState::Ready;
        current_task = "Offline Mode active";
        error_message.clear();
    }
    else
    {
        offline_override = false;
        current_state = SetupState::Error;
        error_message = "Offline mode: This installation is incomplete. AI generation is unavailable until the required components are installed.";
    }
}

void SetupManager::SetVerificationFailure(std::string_view filename, std::string_view exp, std::string_view rec,
                                         std::string_view src, std::string_view err)
{
    verify_fail.has_failure = true;
    verify_fail.filename = std::string(filename);
    verify_fail.expected_sha256 = std::string(exp);
    verify_fail.received_sha256 = std::string(rec);
    verify_fail.source = std::string(src);
    verify_fail.error_code = std::string(err);
}

void SetupManager::ClearVerificationFailure()
{
    verify_fail = {};
}

std::string SetupManager::GetStatusSummary() const
{
    std::string summary = "AI Runtime:\n";
    summary += checklist.motion_model_ready ? "  [OK] Motion Model (SOMA RP v1.1)\n" : "  [MISSING] Motion Model\n";
    summary += checklist.text_encoder_ready ? "  [OK] Text Encoder (LLM2Vec 35 files)\n" : "  [MISSING] Text Encoder\n";
    summary += checklist.character_assets_ready ? "  [OK] Character Assets (CesiumMan)\n"
                                               : "  [MISSING] Character Assets\n";
    summary += checklist.runtime_ready ? ("  [OK] Runtime (" + checklist.gpu_name + ")\n") : "  [WARN] Runtime\n";
    summary += checklist.gpu_ready ? ("  [OK] GPU (" + checklist.gpu_name + ")\n") : "  [WARN] GPU Acceleration\n";
    summary += checklist.hf_connected ? ("  [OK] Hugging Face (@" + checklist.hf_username + ")\n")
                                      : "  [--] Hugging Face (Guest / Not Signed In)\n";
    summary += "Status: " + (IsReady() ? std::string("READY - Start generating") : std::string("SETUP INCOMPLETE"));
    return summary;
}

} // namespace studio
