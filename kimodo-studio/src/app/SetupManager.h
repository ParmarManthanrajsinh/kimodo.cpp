#pragma once

#include <chrono>
#include <string>
#include <vector>
#include "app/AppState.h"
#include "huggingface/HuggingFaceClient.h"
#include "models/ModelManager.h"

namespace studio
{

enum class SetupState
{
    Welcome,
    CheckingSystem,
    NeedsHuggingFaceLogin,
    Authenticating,
    CheckingModelAccess,
    NeedsModelAccess,
    Downloading,
    Verifying,
    Installing,
    CheckingRuntime,
    Ready,
    Error
};

struct SetupChecklist
{
    bool motion_model_ready = false;
    bool text_encoder_ready = false;
    bool character_assets_ready = false;
    bool runtime_ready = false;
    bool gpu_ready = false;
    bool memory_ready = false;
    bool hf_connected = false;

    std::string motion_model_path;
    std::string text_encoder_path;
    std::string hf_username;
    std::string gpu_name;
    std::string runtime_details;
    std::string gpu_status_message;
    std::string memory_details;
    std::string backend_name;
    std::string vram_info;
    std::string runtime_version;
    std::vector<std::string> missing_components;
    std::vector<std::pair<std::string, std::string>> missing_access_models; // {name, repo}
};

struct VerificationFailureDetails
{
    bool has_failure = false;
    std::string filename;
    std::string expected_sha256;
    std::string received_sha256;
    std::string source;
    std::string error_code;
};

class SetupManager
{
public:
    SetupManager() = default;

    void Init(ModelManager& models, AppState& app_state);
    void Update(ModelManager& models, AppState& app_state);
    void CheckStatus(ModelManager& models, AppState& app_state);

    SetupState GetState() const
    {
        return current_state;
    }
    const SetupChecklist& GetChecklist() const
    {
        return checklist;
    }
    bool IsReady() const
    {
        return current_state == SetupState::Ready;
    }
    bool IsBusy() const
    {
        return current_state == SetupState::Downloading || current_state == SetupState::Verifying ||
               current_state == SetupState::Installing || current_state == SetupState::Authenticating;
    }

    float GetProgress() const
    {
        return progress;
    }
    const std::string& GetCurrentTask() const
    {
        return current_task;
    }
    const std::string& GetErrorMessage() const
    {
        return error_message;
    }

    // First-run workflow
    void StartFirstRun(ModelManager& models, AppState& app_state);
    void ReturnToSetup();

    // Device code OAuth
    bool StartDeviceLogin(std::string& error_out);
    void CancelDeviceLogin();
    const HuggingFaceClient::DeviceAuthInit& GetDeviceAuth() const
    {
        return device_auth;
    }

    // Model Access verification
    void CheckModelAccessAll(ModelManager& models);
    const std::vector<std::pair<std::string, std::string>>& GetMissingAccessModels() const
    {
        return checklist.missing_access_models;
    }

    // Component download & installation
    void StartDownloadMissing(ModelManager& models);
    void CancelDownload(ModelManager& models);
    // Verifies EVERY required asset (motion, full text bundle, character,
    // runtime entries), not just the motion model. Sequential queue because
    // ModelManager handles one task at a time.
    void VerifyAll(ModelManager& models);

    // Speed & ETA tracking
    float GetDownloadSpeedMbps() const
    {
        return download_speed_mbps;
    }
    int GetEtaSeconds() const
    {
        return eta_seconds;
    }
    uint64_t GetBytesDone() const
    {
        return bytes_done;
    }
    uint64_t GetBytesTotal() const
    {
        return bytes_total;
    }
    const std::string& GetCurrentFileName() const
    {
        return current_file_name;
    }

    // Verification Failure Details (clean UX)
    const VerificationFailureDetails& GetVerificationFailureDetails() const
    {
        return verify_fail;
    }
    void SetVerificationFailure(std::string_view filename, std::string_view exp, std::string_view rec,
                                std::string_view src, std::string_view err);
    void ClearVerificationFailure();

    // Authentication actions
    bool SignInHF(std::string_view token, std::string& error_out);
    void SignOutHF(AppState& app_state);
    bool CheckHFConnection(std::string& error_out);
    bool VerifyHFAccess(std::string_view repo, std::string& error_out);

    void Retry(ModelManager& models, AppState& app_state);
    void ContinueOffline(ModelManager& models, AppState& app_state);
    void ContinueOffline();

    std::string GetStatusSummary() const;

private:
    SetupState current_state = SetupState::Welcome;
    SetupChecklist checklist;
    float progress = 0.0f;
    std::string current_task = "Idle";
    std::string error_message;
    std::string active_download_id;
    bool offline_override = false;

    // Device OAuth state
    HuggingFaceClient::DeviceAuthInit device_auth;
    std::chrono::steady_clock::time_point last_device_poll{};
    int device_poll_interval = 5;

    // Speed and ETA metrics
    float download_speed_mbps = 0.0f;
    int eta_seconds = 0;
    uint64_t bytes_done = 0;
    uint64_t bytes_total = 0;
    uint64_t last_bytes = 0;
    std::chrono::steady_clock::time_point last_speed_time{};
    std::string current_file_name;

    // Detailed verification failure info
    VerificationFailureDetails verify_fail;

    // Sequential verification queue (ModelManager handles one task at a time).
    std::vector<std::string> verify_queue;
    std::string verify_current;

    // Auto-restart guard: the Downloading state machine re-queues the next
    // missing asset automatically. Cap consecutive restarts of the SAME asset
    // so a persistently failing download parks in Error instead of looping
    // (and re-downloading from zero) forever.
    std::string last_auto_asset;
    int auto_restart_count = 0;
    static constexpr int kMaxAutoRestarts = 3;
    void ResetAutoRestart();
    // Returns false when the cap is hit (caller must enter Error).
    bool NoteAutoRestart(std::string_view asset_id);
};

} // namespace studio
