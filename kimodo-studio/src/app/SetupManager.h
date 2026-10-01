#pragma once

#include <string>
#include "app/AppState.h"
#include "models/ModelManager.h"

namespace studio
{

enum class SetupState
{
    Checking,
    NeedsLogin,
    Downloading,
    Verifying,
    Ready,
    Error
};

struct SetupChecklist
{
    bool motion_model_ready = false;
    bool text_encoder_ready = false;
    bool character_assets_ready = false;
    bool runtime_ready = false;
    bool hf_connected = false;

    std::string motion_model_path;
    std::string text_encoder_path;
    std::string hf_username;
    std::string gpu_name;
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
        return current_state == SetupState::Downloading || current_state == SetupState::Verifying;
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

    // Actions
    void StartDownloadMissing(ModelManager& models);
    void CancelDownload(ModelManager& models);
    void VerifyAll(ModelManager& models);

    bool SignInHF(const std::string& token, std::string& error_out);
    void SignOutHF(AppState& app_state);
    bool CheckHFConnection(std::string& error_out);

    void Retry(ModelManager& models, AppState& app_state);
    void ContinueOffline();

    std::string GetStatusSummary() const;

private:
    SetupState current_state = SetupState::Checking;
    SetupChecklist checklist;
    float progress = 0.0f;
    std::string current_task = "Idle";
    std::string error_message;
    std::string active_download_id;
    bool offline_override = false;
};

} // namespace studio
