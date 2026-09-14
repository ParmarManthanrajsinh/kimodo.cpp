#include "ui/UIManager.h"

#include "animation/AnimationPlayer.h"
#include "huggingface/HFAuthenticator.h"
#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "library/AnimationLibrary.h"
#include "models/ModelManager.h"
#include "raylib.h"
#include "rendering/Viewport.h"
#include "ui/Toast.h"

#include <cstring>
#include <filesystem>

namespace studio {
namespace {

const char* screenLabel(Screen s) {
    switch (s) {
        case Screen::Home: return "Home";
        case Screen::Generate: return "Generate";
        case Screen::Models: return "Models";
        case Screen::Library: return "Library";
        case Screen::Settings: return "Settings";
    }
    return "Home";
}

bool fileStatus(const std::string& path, std::string& detail) {
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec) {
        detail = "missing";
        return false;
    }
    const double gb = static_cast<double>(bytes) / 1e9;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f GB present", gb);
    detail = buf;
    return true;
}

void openEntry(AnimationLibrary& library, const LibraryEntry& e, AnimationPlayer& player,
               Toasts& toasts) {
    MotionResult m;
    if (!library.loadMotion(e, m)) {
        toasts.push("Could not open animation", ToastKind::Error);
        return;
    }
    Animation anim;
    anim.fromMotionResult(m, e.fps);
    player.load(anim);
    toasts.push("Animation opened", ToastKind::Success);
}

void drawHome(AppState& state, AnimationLibrary& library, AnimationPlayer& player,
              Toasts& toasts) {
    ImGui::Text("Kimodo Studio");
    ImGui::TextDisabled("Generate motion from text.");
    ImGui::Spacing();
    if (ImGui::Button("Generate Animation")) {
        state.screen = Screen::Generate;
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Recent Animations");
    const auto& entries = library.entries();
    if (entries.empty()) {
        ImGui::TextDisabled("No animations yet. Generate one to start the library.");
    }
    const size_t show = std::min<size_t>(entries.size(), 3);
    for (size_t i = 0; i < show; ++i) {
        const LibraryEntry& e = entries[entries.size() - 1 - i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextWrapped("%s", e.prompt.c_str());
        ImGui::TextDisabled("%d frames - %s", e.frames, e.createdAt.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Open")) {
            openEntry(library, e, player, toasts);
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::Separator();
    std::string detail;
    if (fileStatus(state.motionPath, detail)) {
        ImGui::Text("Model ready (%s)", detail.c_str());
    } else {
        ImGui::Text("No models installed.");
        ImGui::TextDisabled("Install a model to start generating animations.");
        if (ImGui::Button("Browse Models")) {
            state.screen = Screen::Models;
        }
    }
}

void drawGenerate(AppState& state, KimodoEngine& engine) {
    static char promptBuf[1024];
    static bool promptInit = false;
    if (!promptInit) {
        std::strncpy(promptBuf, state.prompt.c_str(), sizeof(promptBuf) - 1);
        promptBuf[sizeof(promptBuf) - 1] = '\0';
        promptInit = true;
    }
    ImGui::TextWrapped("Model: SOMA RP v1.1 (local)");
    ImGui::Spacing();
    ImGui::InputTextMultiline("Prompt", promptBuf, sizeof(promptBuf), ImVec2(-1, 64));
    state.prompt = promptBuf;
    ImGui::InputInt("Frames", &state.frames);
    ImGui::InputInt("Steps", &state.steps);
    if (state.frames < 8) state.frames = 8;
    if (state.frames > 600) state.frames = 600;
    if (state.steps < 1) state.steps = 1;
    if (state.steps > 200) state.steps = 200;

    const bool busy = engine.busy();
    if (busy) {
        ImGui::BeginDisabled();
        ImGui::Button("Generating...");
        ImGui::EndDisabled();
        if (engine.sampling()) {
            char label[64];
            std::snprintf(label, sizeof(label), "step %u / %u", engine.stepsDone(),
                          engine.stepsTotal());
            ImGui::ProgressBar(engine.progress(), ImVec2(-1, 0), label);
        } else {
            ImGui::TextDisabled("Preparing (weights / encoding)...");
        }
        if (ImGui::Button("Cancel")) {
            engine.cancel();
        }
    } else if (ImGui::Button("Generate")) {
        GenerationParams params;
        params.frames = static_cast<uint32_t>(state.frames);
        params.steps = static_cast<uint32_t>(state.steps);
        params.seed = state.seed;
        engine.requestGenerate(state.prompt, params);
    }
    ImGui::Spacing();
    ImGui::TextWrapped("Status: %s", engine.message().c_str());
}

void drawModels(AppState& state, ModelManager& models, KimodoEngine& engine,
                Toasts& toasts) {
    if (models.busy()) {
        ImGui::ProgressBar(models.taskProgress(), ImVec2(-1, 0),
                           models.taskLabel().c_str());
        if (ImGui::SmallButton("Cancel")) {
            models.cancelTask();
        }
        ImGui::Spacing();
    } else if (models.task() == ModelTask::None && !models.taskLabel().empty() &&
               models.taskLabel() != "idle") {
        ImGui::TextWrapped("%s", models.taskLabel().c_str());
        ImGui::Spacing();
    }

    ImGui::Text("Installed");
    ImGui::Separator();
    const std::vector<ModelEntry> entries = models.entries();
    const std::string activeId = models.activeId();
    static std::string confirmDelete;
    static char importBuf[1024] = "";
    for (size_t i = 0; i < entries.size(); ++i) {
        const ModelEntry& e = entries[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("%s  %s", e.name.c_str(),
                    e.id == activeId ? "(active)" : "");
        ImGui::TextDisabled("%s - v%s - %s", e.skeleton.c_str(), e.version.c_str(),
                            e.license.c_str());
        if (e.installed) {
            ImGui::TextDisabled("%.2f GB - %s",
                                static_cast<double>(e.localBytes) / 1e9,
                                e.localPath.c_str());
            if (e.id != activeId) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Select") && !models.busy()) {
                    if (models.select(e.id)) {
                        toasts.push("Model selected: " + e.name, ToastKind::Success);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Verify") && !models.busy() && !engine.busy()) {
                models.verifyAsync(e.id);
            }
            ImGui::SameLine();
            if (confirmDelete == e.id) {
                if (ImGui::SmallButton("Confirm?")) {
                    models.deleteAsync(e.id);
                    confirmDelete.clear();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Keep")) {
                    confirmDelete.clear();
                }
            } else if (ImGui::SmallButton("Delete") && !models.busy()) {
                confirmDelete = e.id;
            }
        } else {
            ImGui::TextDisabled("not installed (%.2f GB)",
                                static_cast<double>(e.sizeBytes) / 1e9);
            if (!e.repo.empty()) {
                ImGui::TextDisabled("from %s", e.repo.c_str());
                if (ImGui::SmallButton("Download") && !models.busy()) {
                    models.downloadAsync(e.id);
                }
            }
            ImGui::InputText("Local .gguf", importBuf, sizeof(importBuf));
            ImGui::SameLine();
            if (ImGui::SmallButton("Import") && !models.busy() && importBuf[0]) {
                models.importAsync(importBuf, e.id);
            }
        }
        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::Text("Text bundle");
    ImGui::Separator();
    std::error_code ec;
    const bool bundleOk = std::filesystem::is_directory(state.textBundle, ec) && !ec;
    ImGui::TextDisabled("%s", bundleOk ? "present" : "missing");
    ImGui::TextDisabled("%s", state.textBundle.c_str());
    ImGui::Spacing();
    ImGui::TextDisabled("Hugging Face downloads arrive in Phase 6.");
}

void drawLibrary(AppState& state, AnimationLibrary& library, AnimationPlayer& player,
                 Toasts& toasts) {
    (void)state;
    const auto& entries = library.entries();
    ImGui::Text("Animations (%llu)", static_cast<unsigned long long>(entries.size()));
    ImGui::Separator();
    if (entries.empty()) {
        ImGui::TextDisabled("Library empty. Finished generations auto-save here.");
        return;
    }
    static std::string renameId;
    static char renameBuf[1024];
    for (size_t i = entries.size(); i-- > 0;) {
        const LibraryEntry& e = entries[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextWrapped("%s", e.prompt.c_str());
        ImGui::TextDisabled("%d frames - %s", e.frames, e.createdAt.c_str());
        if (ImGui::SmallButton("Open")) {
            openEntry(library, e, player, toasts);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Duplicate")) {
            if (library.duplicate(e.id)) {
                toasts.push("Animation duplicated", ToastKind::Success);
            } else {
                toasts.push("Duplicate failed", ToastKind::Error);
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Rename")) {
            renameId = e.id;
            std::strncpy(renameBuf, e.prompt.c_str(), sizeof(renameBuf) - 1);
            renameBuf[sizeof(renameBuf) - 1] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Export")) {
            toasts.push("Export arrives in Phase 10 (GLB first)", ToastKind::Warning);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete")) {
            if (library.remove(e.id)) {
                toasts.push("Animation deleted", ToastKind::Info);
            } else {
                toasts.push("Delete failed", ToastKind::Error);
            }
            ImGui::PopID();
            break; // list mutated; restart next frame
        }
        if (renameId == e.id) {
            ImGui::InputText("##rename", renameBuf, sizeof(renameBuf));
            ImGui::SameLine();
            if (ImGui::SmallButton("Save")) {
                if (library.rename(e.id, renameBuf)) {
                    toasts.push("Animation renamed", ToastKind::Success);
                } else {
                    toasts.push("Rename failed", ToastKind::Error);
                }
                renameId.clear();
            }
        }
        ImGui::Separator();
        ImGui::PopID();
    }
}

void drawSettings(AppState& state, KimodoEngine& engine, Toasts& toasts) {
    ImGui::Text("Models");
    ImGui::Separator();
    static char motionBuf[1024];
    static char bundleBuf[1024];
    static bool settingsInit = false;
    if (!settingsInit) {
        std::strncpy(motionBuf, state.motionPath.c_str(), sizeof(motionBuf) - 1);
        motionBuf[sizeof(motionBuf) - 1] = '\0';
        std::strncpy(bundleBuf, state.textBundle.c_str(), sizeof(bundleBuf) - 1);
        bundleBuf[sizeof(bundleBuf) - 1] = '\0';
        settingsInit = true;
    }
    ImGui::InputText("Motion model", motionBuf, sizeof(motionBuf));
    ImGui::InputText("Text bundle", bundleBuf, sizeof(bundleBuf));
    if (ImGui::Button("Apply paths")) {
        state.motionPath = motionBuf;
        state.textBundle = bundleBuf;
        engine.setPaths(state.motionPath, state.textBundle);
        engine.unloadModel();
        toasts.push("Paths applied, model unloaded", ToastKind::Success);
    }
    ImGui::Spacing();
    ImGui::Text("Generation defaults");
    ImGui::Separator();
    ImGui::InputInt("Frames", &state.frames);
    ImGui::InputInt("Steps", &state.steps);
    int seed = static_cast<int>(state.seed);
    if (ImGui::InputInt("Seed", &seed) && seed >= 0) {
        state.seed = static_cast<unsigned long long>(seed);
    }
    ImGui::Spacing();
    ImGui::Text("Hugging Face");
    ImGui::Separator();
    if (!state.hfUser.empty()) {
        ImGui::Text("Status: Connected");
        ImGui::Text("Account: %s", state.hfUser.c_str());
        if (ImGui::Button("Disconnect")) {
            HFAuthenticator::clearToken();
            state.hfUser.clear();
            toasts.push("Hugging Face disconnected", ToastKind::Info);
        }
    } else {
        static char tokenBuf[512] = "";
        static bool hasSaved = false;
        static bool savedChecked = false;
        if (!savedChecked) {
            std::string probe;
            hasSaved = HFAuthenticator::loadToken(probe);
            savedChecked = true;
        }
        if (hasSaved) {
            ImGui::TextDisabled("Saved token present (not verified).");
            if (ImGui::Button("Verify")) {
                std::string saved, error;
                if (HFAuthenticator::loadToken(saved)) {
                    const std::string user = HFAuthenticator::validate(saved, error);
                    if (!user.empty()) {
                        state.hfUser = user;
                        toasts.push("Connected as " + user, ToastKind::Success);
                    } else {
                        toasts.push("Token invalid: " + error, ToastKind::Error);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Forget")) {
                HFAuthenticator::clearToken();
                hasSaved = false;
            }
        } else {
            ImGui::TextDisabled("Paste a Hugging Face access token.");
            ImGui::InputText("Token", tokenBuf, sizeof(tokenBuf),
                             ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            if (ImGui::Button("Connect") && tokenBuf[0]) {
                std::string error;
                const std::string user =
                    HFAuthenticator::validate(tokenBuf, error);
                if (!user.empty() && HFAuthenticator::saveToken(tokenBuf, error)) {
                    state.hfUser = user;
                    hasSaved = true;
                    tokenBuf[0] = '\0';
                    toasts.push("Connected as " + user, ToastKind::Success);
                } else {
                    toasts.push("Connect failed: " + error, ToastKind::Error);
                }
            }
        }
    }
    ImGui::Spacing();
    ImGui::Text("Performance");
    ImGui::Separator();
    ImGui::TextDisabled("GPU auto-detect, VRAM strategy and CPU offload arrive in Phase 7.");
}

} // namespace

void UIManager::draw(AppState& state, Viewport& viewport, KimodoEngine& engine,
                     AnimationPlayer& player, AnimationLibrary& library,
                     ModelManager& models, Toasts& toasts) {
    const float topH = 36.0f;
    const float sideW = 180.0f;
    const float statusH = 26.0f;
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();

    // Top bar
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)sw, topH));
    ImGui::Begin("TopBar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::TextUnformatted("Kimodo Studio");
    ImGui::SameLine(sw - 220.0f);
    ImGui::TextDisabled("GPU");
    ImGui::SameLine();
    ImGui::TextUnformatted("ON");
    ImGui::End();

    // Sidebar
    ImGui::SetNextWindowPos(ImVec2(0, topH));
    ImGui::SetNextWindowSize(ImVec2(sideW, (float)sh - topH - statusH));
    ImGui::Begin("Sidebar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    const Screen items[] = {Screen::Home, Screen::Generate, Screen::Models,
                            Screen::Library, Screen::Settings};
    for (Screen item : items) {
        if (ImGui::Selectable(screenLabel(item), state.screen == item)) {
            state.screen = item;
        }
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Reset camera (R)")) {
        viewport.reset();
    }
    ImGui::End();

    // Content panel per screen
    ImGui::SetNextWindowPos(ImVec2(sideW, topH));
    ImGui::SetNextWindowSize(ImVec2(300.0f, (float)sh - topH - statusH));
    ImGui::Begin("Panel", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::Text("%s", screenLabel(state.screen));
    ImGui::Separator();
    switch (state.screen) {
        case Screen::Home: drawHome(state, library, player, toasts); break;
        case Screen::Generate: drawGenerate(state, engine); break;
        case Screen::Models: drawModels(state, models, engine, toasts); break;
        case Screen::Library: drawLibrary(state, library, player, toasts); break;
        case Screen::Settings: drawSettings(state, engine, toasts); break;
    }
    ImGui::Spacing();
    ImGui::Text("Camera dist: %.1f", viewport.distance());
    if (ImGui::Button("Frame (F)")) {
        viewport.frame();
    }
    ImGui::End();

    // Timeline (visible once an animation is loaded)
    const float timelineH = 64.0f;
    if (player.hasAnimation()) {
        ImGui::SetNextWindowPos(ImVec2(sideW, (float)sh - statusH - timelineH));
        ImGui::SetNextWindowSize(ImVec2((float)sw - sideW, timelineH));
        ImGui::Begin("Timeline", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        if (ImGui::Button(player.playing() ? "Pause" : "Play")) {
            player.toggle();
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart")) {
            player.restart();
        }
        ImGui::SameLine();
        bool loop = player.loop();
        if (ImGui::Checkbox("Loop", &loop)) {
            player.setLoop(loop);
        }
        ImGui::SameLine();
        float t = player.time();
        if (ImGui::SliderFloat("##scrub", &t, 0.0f, player.duration(), "%.2fs")) {
            player.scrub(t);
        }
        ImGui::SameLine();
        ImGui::Text("f %d / %.0f fps", player.frame(), player.fps());
        ImGui::End();
    }

    // Status bar
    ImGui::SetNextWindowPos(ImVec2(0, (float)sh - statusH));
    ImGui::SetNextWindowSize(ImVec2((float)sw, statusH));
    ImGui::Begin("Status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (player.hasAnimation()) {
        ImGui::Text("FPS %d | frame %d | %s", state.fps, player.frame(),
                    engine.message().c_str());
    } else {
        ImGui::Text("FPS %d | %s | %s", state.fps, state.gpuName.c_str(),
                    engine.message().c_str());
    }
    ImGui::End();
}

} // namespace studio
