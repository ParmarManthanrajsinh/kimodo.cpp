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
    {
        std::lock_guard<std::mutex> lock(mutex_);
        message_ = "starting worker";
    }
    worker_ = std::thread(&KimodoEngine::run, this, std::move(prompt), params);
}

std::string KimodoEngine::message() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return message_;
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
        message_ = "Encoding prompt / sampling...";
    }
    Logger::instance().info("Kimodo: generation started: " + prompt);
    MotionResult result;
    std::string error;
    if (!adapter_.generate(prompt, params, result, error)) {
        std::lock_guard<std::mutex> lock(mutex_);
        message_ = "Generation failed: " + error;
        status_.store(EngineStatus::Error);
        Logger::instance().error("Kimodo generate failed: " + error);
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
