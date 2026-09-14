#include "ui/UIManager.h"

#include "animation/AnimationPlayer.h"
#include "huggingface/HFAuthenticator.h"
#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "library/AnimationLibrary.h"
#include "models/ModelManager.h"
#include "raylib.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "rendering/Viewport.h"
#include "rlImGui.h"
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
        case Screen::Retarget: return "Retarget";
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
    Animation anim;
    if (!library.loadAnimation(e, anim)) {
        toasts.push("Could not open animation", ToastKind::Error);
        return;
    }
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

void drawRetarget(AppState& state, AnimationLibrary& library, AnimationPlayer& player,
                  Toasts& toasts) {
    const std::vector<LibraryEntry> entries = library.entries();
    if (entries.empty()) {
        ImGui::TextDisabled("Library empty. Generate an animation first.");
        return;
    }
    static std::string targetId = "unreal-manny";
    static BoneMap map;
    static bool mapInit = false;
    static float rootScale = 1.0f;
    static std::string cachedSource;
    static Animation sourceAnim;

    // Source combo.
    int srcIdx = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].id == state.retargetSource) {
            srcIdx = static_cast<int>(i);
        }
    }
    if (state.retargetSource.empty()) {
        state.retargetSource = entries.back().id;
        srcIdx = static_cast<int>(entries.size() - 1);
    }
    if (ImGui::BeginCombo("Source", entries[srcIdx].prompt.c_str())) {
        for (size_t i = 0; i < entries.size(); ++i) {
            const bool sel = (static_cast<int>(i) == srcIdx);
            if (ImGui::Selectable(entries[i].prompt.c_str(), sel)) {
                state.retargetSource = entries[i].id;
            }
            if (sel) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    const LibraryEntry& src = entries[srcIdx];

    // Target combo.
    const std::vector<SkeletonProfile>& profiles = targetProfiles();
    int tgtIdx = 0;
    for (size_t i = 0; i < profiles.size(); ++i) {
        if (profiles[i].id == targetId) {
            tgtIdx = static_cast<int>(i);
        }
    }
    if (ImGui::BeginCombo("Target", profiles[tgtIdx].name.c_str())) {
        for (size_t i = 0; i < profiles.size(); ++i) {
            const bool sel = (static_cast<int>(i) == tgtIdx);
            if (ImGui::Selectable(profiles[i].name.c_str(), sel)) {
                targetId = profiles[i].id;
                mapInit = false;
            }
            if (sel) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    const SkeletonProfile& profile = profiles[tgtIdx];

    // (Re)load source topology when selection changes.
    if (!mapInit || cachedSource != src.id) {
        if (library.loadAnimation(src, sourceAnim)) {
            map = Retargeter::autoMap(profile);
            mapInit = true;
            cachedSource = src.id;
        } else {
            ImGui::TextDisabled("Could not load source animation.");
            return;
        }
    }

    const std::vector<std::string> missing = Retargeter::unmapped(profile, map);
    ImGui::Text("Source skeleton: %s", sourceAnim.skeletonName.c_str());
    if (!missing.empty()) {
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "%llu joints need manual mapping",
                           static_cast<unsigned long long>(missing.size()));
    } else {
        ImGui::TextDisabled("All joints mapped.");
    }
    if (ImGui::Button("Auto Map")) {
        map = Retargeter::autoMap(profile);
    }
    ImGui::SameLine();
    ImGui::SliderFloat("Root scale", &rootScale, 0.5f, 1.5f);

    ImGui::Separator();
    ImGui::Text("Mapping");
    std::vector<std::string> items = {"(none)"};
    for (const std::string& n : sourceAnim.jointNames) {
        items.push_back(n);
    }
    std::vector<const char*> itemPtrs;
    for (const std::string& n : items) {
        itemPtrs.push_back(n.c_str());
    }
    for (const std::string& tgt : profile.joints) {
        ImGui::PushID(tgt.c_str());
        int cur = 0;
        const auto it = map.find(tgt);
        if (it != map.end()) {
            for (size_t k = 0; k < items.size(); ++k) {
                if (items[k] == it->second) {
                    cur = static_cast<int>(k);
                }
            }
        }
        if (ImGui::Combo(tgt.c_str(), &cur, itemPtrs.data(),
                         static_cast<int>(itemPtrs.size()))) {
            map[tgt] = items[cur];
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("Preview")) {
        Animation out;
        std::string error;
        Retargeter::Options opts;
        opts.rootScale = rootScale;
        if (Retargeter::retarget(sourceAnim, profile, map, opts, out, error)) {
            player.load(out);
            toasts.push("Retarget preview loaded", ToastKind::Success);
        } else {
            toasts.push("Retarget failed: " + error, ToastKind::Error);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply (save)")) {
        Animation out;
        std::string error;
        Retargeter::Options opts;
        opts.rootScale = rootScale;
        if (Retargeter::retarget(sourceAnim, profile, map, opts, out, error)) {
            LibraryEntry saved;
            if (library.saveAnimation(src.prompt + " [" + profile.id + "]", src.model,
                                      out, saved)) {
                toasts.push("Retargeted animation saved", ToastKind::Success);
            } else {
                toasts.push("Save failed", ToastKind::Error);
            }
        } else {
            toasts.push("Retarget failed: " + error, ToastKind::Error);
        }
    }
}

void drawLibrary(UIManager* self, AppState& state, AnimationLibrary& library,
                 AnimationPlayer& player, Toasts& toasts, UIManager::CaptureFn& capture) {
    const std::vector<LibraryEntry> entries = library.entries();
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
        self->drawThumb(e);
        ImGui::TextWrapped("%s", e.prompt.c_str());
        const float dur = e.fps > 0 ? static_cast<float>(e.frames) / e.fps : 0.0f;
        ImGui::TextDisabled("%d frames - %.1fs - %s", e.frames, dur,
                            e.createdAt.c_str());
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
        if (capture && ImGui::SmallButton("Thumbnail")) {
            capture(e);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Retarget")) {
            state.retargetSource = e.id;
            state.screen = Screen::Retarget;
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

void UIManager::drawThumb(const LibraryEntry& e) {
    if (thumbs_.size() != thumbCount_) {
        // Library mutated: drop stale GPU textures.
        for (auto& [id, tex] : thumbs_) {
            UnloadTexture(tex);
        }
        thumbs_.clear();
    }
    thumbCount_ = thumbs_.size();
    if (!AnimationLibrary::hasThumb(e)) {
        return;
    }
    auto it = thumbs_.find(e.id);
    if (it == thumbs_.end()) {
        Texture2D tex = LoadTexture(AnimationLibrary::thumbPath(e).string().c_str());
        if (tex.id == 0) {
            return;
        }
        it = thumbs_.emplace(e.id, tex).first;
        thumbCount_ = thumbs_.size();
    }
    rlImGuiImageSize(&it->second, 120, 75);
}

void UIManager::draw(AppState& state, Viewport& viewport, KimodoEngine& engine,
                     AnimationPlayer& player, AnimationLibrary& library,
                     ModelManager& models, Toasts& toasts, CaptureFn capture) {
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
    const Screen items[] = {Screen::Home,       Screen::Generate, Screen::Models,
                            Screen::Library, Screen::Retarget, Screen::Settings};
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
        case Screen::Library:
            drawLibrary(this, state, library, player, toasts, capture);
            break;
        case Screen::Retarget: drawRetarget(state, library, player, toasts); break;
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
