#pragma once

#include <string>

namespace studio {

enum class Screen { Home, Generate, Models, Library, Retarget, Export, Settings };

struct AppState {
    Screen screen = Screen::Retarget;
    std::string gpuName = "RTX 4060";
    int fps = 0;
    bool vulkanAvailable = false;

    // Phase 2: generation inputs (local model, no downloads yet).
    std::string prompt = "A person walks forward.";
    int frames = 120;
    int steps = 50;
    unsigned long long seed = 42;
    std::string motionPath = "E:/kimodo.cpp/models/kimodo-soma-rp-v1.1-f32.gguf";
    std::string textBundle = "E:/kimodo.cpp/generated/llm2vec-text-bundle";
    std::string hfUser; // verified Hugging Face account, empty = not connected
    std::string retargetSource; // library id selected for retargeting
};

} // namespace studio
