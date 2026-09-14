#include "kimodo/KimodoEngine.h"

#include "utils/Logger.h"

namespace studio {

void KimodoEngine::setPaths(std::string motionGguf, std::string textBundle) {
    std::lock_guard<std::mutex> lock(mutex_);
    motionPath_ = std::move(motionGguf);
    textBundle_ = std::move(textBundle);
}

void KimodoEngine::requestGenerate(std::string prompt, GenerationParams params) {
    if (busy()) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    status_.store(EngineStatus::Generating);
    stepsDone_.store(0);
    stepsTotal_.store(params.steps);
    sampling_.store(false);
    cancelRequested_.store(false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        message_ = "starting worker";
        lastPrompt_ = prompt;
    }
    worker_ = std::thread(&KimodoEngine::run, this, std::move(prompt), params);
}

void KimodoEngine::cancel() {
    cancelRequested_.store(true);
}

float KimodoEngine::progress() const {
    const unsigned total = stepsTotal_.load();
    if (total == 0) {
        return 0.0f;
    }
    float p = static_cast<float>(stepsDone_.load()) / static_cast<float>(total);
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

std::string KimodoEngine::message() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return message_;
}

std::string KimodoEngine::lastPrompt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastPrompt_;
}

void KimodoEngine::unloadModel() {
    if (busy()) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    adapter_.unload();
    std::lock_guard<std::mutex> lock(mutex_);
    message_ = "model unloaded (paths apply on next generate)";
    status_.store(EngineStatus::Idle);
}

bool KimodoEngine::busy() const {
    EngineStatus s = status_.load();
    return s == EngineStatus::LoadingModel || s == EngineStatus::Generating;
}

bool KimodoEngine::lastResult(MotionResult& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hasResult_) {
        return false;
    }
    out = result_;
    return true;
}

void KimodoEngine::shutdown() {
    if (worker_.joinable()) {
        worker_.join();
    }
    adapter_.unload();
}

void KimodoEngine::run(std::string prompt, GenerationParams params) {
    std::string motionPath;
    std::string textBundle;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        motionPath = motionPath_;
        textBundle = textBundle_;
    }
    if (!adapter_.isLoaded()) {
        status_.store(EngineStatus::LoadingModel);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            message_ = "Loading model...";
        }
        Logger::instance().info("Kimodo: loading model");
        std::string error;
        if (!adapter_.load(motionPath, textBundle, error)) {
            std::lock_guard<std::mutex> lock(mutex_);
            message_ = "Model load failed: " + error;
            status_.store(EngineStatus::Error);
            Logger::instance().error("Kimodo load failed: " + error);
            return;
        }
        Logger::instance().info("Kimodo: model loaded");
    }

    status_.store(EngineStatus::Generating);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        message_ = "Preparing (weights / encoding)...";
    }
    Logger::instance().info("Kimodo: generation started: " + prompt);
    MotionResult result;
    std::string error;
    auto onProgress = [this](unsigned done, unsigned total) {
        stepsDone_.store(done);
        stepsTotal_.store(total);
        sampling_.store(true);
        return cancelRequested_.load();
    };
    if (!adapter_.generate(prompt, params, result, error, onProgress)) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (error == "generation cancelled" || cancelRequested_.load()) {
            message_ = "Generation cancelled";
            status_.store(EngineStatus::Idle);
            Logger::instance().info("Kimodo: generation cancelled by user");
        } else {
            message_ = "Generation failed: " + error;
            status_.store(EngineStatus::Error);
            Logger::instance().error("Kimodo generate failed: " + error);
        }
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result_ = std::move(result);
        hasResult_ = true;
        message_ = "Finished: " + std::to_string(result_.frames) + " frames, " +
                   std::to_string(result_.joints) + " joints";
    }
    status_.store(EngineStatus::Finished);
    Logger::instance().info("Kimodo: generation finished");
}

} // namespace studio
