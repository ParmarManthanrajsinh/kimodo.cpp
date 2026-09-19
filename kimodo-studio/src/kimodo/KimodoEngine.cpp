#include "kimodo/KimodoEngine.h"

#include "utils/Logger.h"

namespace studio
{

void KimodoEngine::SetPaths(std::string in_motion_gguf, std::string in_text_bundle)
{
    std::lock_guard<std::mutex> lock(mutex);
    motion_path = std::move(in_motion_gguf);
    text_bundle = std::move(in_text_bundle);
}

void KimodoEngine::RequestGenerate(std::string prompt, GenerationParams params)
{
    if (IsBusy())
    {
        return;
    }
    if (worker.joinable())
    {
        worker.join();
    }
    status.store(EngineStatus::Generating);
    steps_done.store(0);
    steps_total.store(params.steps);
    sampling.store(false);
    cancel_requested.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex);
        message = "starting worker";
        last_prompt = prompt;
    }
    worker = std::thread(&KimodoEngine::Run, this, std::move(prompt), params);
}

void KimodoEngine::Cancel()
{
    cancel_requested.store(true);
}

float KimodoEngine::GetProgress() const
{
    const unsigned total = steps_total.load();
    if (total == 0)
    {
        return 0.0f;
    }
    float p = static_cast<float>(steps_done.load()) / static_cast<float>(total);
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

std::string KimodoEngine::GetMessage() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return message;
}

std::string KimodoEngine::GetLastPrompt() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return last_prompt;
}

void KimodoEngine::UnloadModel()
{
    if (IsBusy())
    {
        return;
    }
    if (worker.joinable())
    {
        worker.join();
    }
    adapter.Unload();
    std::lock_guard<std::mutex> lock(mutex);
    message = "model unloaded (paths apply on next generate)";
    status.store(EngineStatus::Idle);
}

bool KimodoEngine::IsBusy() const
{
    EngineStatus s = status.load();
    return s == EngineStatus::LoadingModel || s == EngineStatus::Generating;
}

bool KimodoEngine::LastResult(MotionResult& out) const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!has_result)
    {
        return false;
    }
    out = result;
    return true;
}

void KimodoEngine::Shutdown()
{
    if (worker.joinable())
    {
        worker.join();
    }
    adapter.Unload();
}

void KimodoEngine::Run(std::string prompt, GenerationParams params)
{
    std::string local_motion_path;
    std::string local_text_bundle;
    {
        std::lock_guard<std::mutex> lock(mutex);
        local_motion_path = motion_path;
        local_text_bundle = text_bundle;
    }
    if (!adapter.IsLoaded())
    {
        if (local_motion_path.empty())
        {
            std::lock_guard<std::mutex> lock(mutex);
            message = "No motion model installed. Please open Models page to download or import SOMA weights.";
            status.store(EngineStatus::Error);
            Logger::GetInstance().Error(
                "Kimodo load aborted: No motion model installed (motion_gguf path is empty). Open Models page.");
            return;
        }

        status.store(EngineStatus::LoadingModel);
        {
            std::lock_guard<std::mutex> lock(mutex);
            message = "Loading model...";
        }
        Logger::GetInstance().Info("Kimodo: loading model from " + local_motion_path);
        std::string error;
        if (!adapter.Load(local_motion_path, local_text_bundle, error))
        {
            std::lock_guard<std::mutex> lock(mutex);
            message = "Model load failed: " + error;
            status.store(EngineStatus::Error);
            Logger::GetInstance().Error("Kimodo load failed: " + error);
            return;
        }
        Logger::GetInstance().Info("Kimodo: model loaded successfully");
    }

    status.store(EngineStatus::Generating);
    {
        std::lock_guard<std::mutex> lock(mutex);
        message = "Preparing (weights / encoding)...";
    }
    Logger::GetInstance().Info("Kimodo: generation started: " + prompt);
    MotionResult local_result;
    std::string error;
    auto on_progress = [this](unsigned done, unsigned total) {
        steps_done.store(done);
        steps_total.store(total);
        sampling.store(true);
        return cancel_requested.load();
    };
    if (!adapter.Generate(prompt, params, result, error, on_progress))
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (error == "generation cancelled" || cancel_requested.load())
        {
            message = "Generation cancelled";
            status.store(EngineStatus::Idle);
            Logger::GetInstance().Info("Kimodo: generation cancelled by user");
        }
        else
        {
            message = "Generation failed: " + error;
            status.store(EngineStatus::Error);
            Logger::GetInstance().Error("Kimodo generate failed: " + error);
        }
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
        result = std::move(local_result);
        has_result = true;
        message = "Finished: " + std::to_string(local_result.frames) + " frames, " +
                  std::to_string(local_result.joints) + " joints";
    }
    status.store(EngineStatus::Finished);
    Logger::GetInstance().Info("Kimodo: generation finished");
}

} // namespace studio
