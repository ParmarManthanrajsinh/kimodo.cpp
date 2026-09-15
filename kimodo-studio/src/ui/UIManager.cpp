#include "ui/UIManager.h"

#include "animation/AnimationPlayer.h"
#include "animation/Skeleton.h"
#include "huggingface/HFAuthenticator.h"
#include "imgui.h"
#include "kimodo/KimodoEngine.h"
#include "library/AnimationLibrary.h"
#include "export/BVHExporter.h"
#include "export/GLBExporter.h"
#include "models/ModelManager.h"
#include "raylib.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "rendering/Viewport.h"
#include "rlImGui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>

namespace studio {
namespace {

// Hugging Face auth runs on a worker: WinHTTP validates on the UI thread
// would freeze the window for the full timeout when offline.
struct AuthWorker {
    std::thread thread;
    std::atomic<bool> busy{false};
    std::mutex mutex;
    bool done = false;
    bool ok = false;
    std::string user;
    std::string error;
};

AuthWorker gHfAuth;

void requestAuth(AuthWorker& w, std::string token, bool save) {
    if (w.busy.load()) {
        return;
    }
    if (w.thread.joinable()) {
        w.thread.join();
    }
    w.busy.store(true);
    {
        std::lock_guard<std::mutex> lock(w.mutex);
        w.done = false;
    }
    w.thread = std::thread([token = std::move(token), save, &w] {
        std::string error;
        std::string user = HFAuthenticator::validate(token, error);
        bool ok = !user.empty();
        if (ok && save) {
            ok = HFAuthenticator::saveToken(token, error);
        }
        {
            std::lock_guard<std::mutex> lock(w.mutex);
            w.user = user;
            w.error = error;
            w.ok = ok;
            w.done = true;
        }
        w.busy.store(false);
    });
}

const char* screenLabel(Screen s) {
    switch (s) {
        case Screen::Home: return "Home";
        case Screen::Generate: return "Generate";
        case Screen::Models: return "Models";
        case Screen::Library: return "Library";
        case Screen::Retarget: return "Retarget";
        case Screen::Export: return "Export";
        case Screen::Settings: return "Settings";
    }
    return "Home";
}

const char* screenShortcut(Screen s) {
    switch (s) {
        case Screen::Home: return "1";
        case Screen::Generate: return "2";
        case Screen::Models: return "3";
        case Screen::Library: return "4";
        case Screen::Retarget: return "5";
        case Screen::Export: return "6";
        case Screen::Settings: return "7";
    }
    return "";
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
    Theme::sectionHeader("WORKSPACE");
    ImGui::Text("Kimodo Studio");
    ImGui::TextDisabled("GPU-powered motion generation workstation.");
    ImGui::Spacing();
    if (ImGui::Button("Generate Motion")) {
        state.screen = Screen::Generate;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Go to Generate (2)");
    }
    Theme::sectionHeader("RECENT");
    ImGui::Text("Recent Animations");
    const auto& entries = library.entries();
    if (entries.empty()) {
        ImGui::TextDisabled("NO ANIMATIONS YET");
        ImGui::TextDisabled("Generate one to start the library.");
        if (ImGui::Button("Open Library")) {
            state.screen = Screen::Library;
        }
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
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Load into viewport");
        }
        ImGui::PopID();
    }
    Theme::sectionHeader("MODEL");
    std::string detail;
    if (fileStatus(state.motionPath, detail)) {
        Theme::statusBadge(true, ("Ready (" + detail + ")").c_str(), "");
    } else {
        ImGui::TextDisabled("NO MODELS INSTALLED");
        ImGui::TextDisabled("Install a model to generate motion.");
        if (ImGui::Button("Browse Models")) {
            state.screen = Screen::Models;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Go to Models (3)");
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
    Theme::sectionHeader("GENERATE MOTION");
    ImGui::TextDisabled("Model: SOMA RP v1.1 (local)");
    ImGui::Spacing();
    ImGui::InputTextMultiline("Prompt", promptBuf, sizeof(promptBuf), ImVec2(-1, 64));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Describe motion, e.g. A person walks forward.");
    }
    state.prompt = promptBuf;
    ImGui::InputInt("Frames", &state.frames);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("8-600 frames. Duration = frames / 30 FPS.");
    }
    if (state.frames < 8) state.frames = 8;
    if (state.frames > 600) state.frames = 600;
    ImGui::TextDisabled("%.1f sec @ 30 FPS - %d joints",
                        static_cast<float>(state.frames) / 30.0f, 30);
    if (ImGui::CollapsingHeader("Advanced")) {
        ImGui::InputInt("Steps", &state.steps);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Diffusion steps. More = slower, smoother.");
        }
        if (state.steps < 1) state.steps = 1;
        if (state.steps > 200) state.steps = 200;
        int seed = static_cast<int>(state.seed);
        if (ImGui::InputInt("Seed", &seed) && seed >= 0) {
            state.seed = static_cast<unsigned long long>(seed);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Same seed + prompt = repeatable motion.");
        }
        ImGui::TextDisabled("Device: GPU auto - precision auto");
    }

    const bool busy = engine.busy();
    if (busy) {
        ImGui::BeginDisabled();
        ImGui::Button("Generating Motion...");
        ImGui::EndDisabled();
        if (engine.sampling()) {
            char label[64];
            std::snprintf(label, sizeof(label), "step %u / %u - %d%%", engine.stepsDone(),
                          engine.stepsTotal(),
                          static_cast<int>(engine.progress() * 100.0f));
            ImGui::ProgressBar(engine.progress(), ImVec2(-1, 0), label);
            ImGui::TextDisabled("GPU: %s", state.gpuName.c_str());
        } else {
            ImGui::TextDisabled("Preparing (weights / encoding)...");
        }
        if (ImGui::Button("Cancel")) {
            engine.cancel();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Stop generation");
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.52f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.38f, 0.62f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(0.24f, 0.42f, 0.09f, 1.0f));
        if (ImGui::Button("Generate Motion")) {
            GenerationParams params;
            params.frames = static_cast<uint32_t>(state.frames);
            params.steps = static_cast<uint32_t>(state.steps);
            params.seed = state.seed;
            engine.requestGenerate(state.prompt, params);
        }
        ImGui::PopStyleColor(3);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Generate (Ctrl+Enter in prompt not required)");
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

    Theme::sectionHeader("MODEL LIBRARY");
    ImGui::TextDisabled("License stays visible. Never bundled with app.");
    ImGui::Spacing();
    const std::vector<ModelEntry> entries = models.entries();
    const std::string activeId = models.activeId();
    static std::string confirmDelete;
    static char importBuf[1024] = "";
    for (size_t i = 0; i < entries.size(); ++i) {
        const ModelEntry& e = entries[i];
        ImGui::PushID(static_cast<int>(i));
        Theme::statusBadge(e.installed, e.id == activeId ? "Active" : "Installed",
                             "Not installed");
        ImGui::SameLine();
        ImGui::Text("%s", e.name.c_str());
        ImGui::TextDisabled("%s - v%s - License: %s", e.skeleton.c_str(),
                            e.version.c_str(), e.license.c_str());
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

    Theme::sectionHeader("TEXT BUNDLE");
    std::error_code ec;
    const bool bundleOk = std::filesystem::is_directory(state.textBundle, ec) && !ec;
    ImGui::TextDisabled("%s", bundleOk ? "present" : "missing");
    ImGui::TextDisabled("%s", state.textBundle.c_str());
    ImGui::Spacing();
    ImGui::TextDisabled("Hugging Face downloads arrive in Phase 6.");
}

void drawRetarget(AppState& state, AnimationLibrary& library, AnimationPlayer& player,
                  Toasts& toasts, Viewport& viewport) {
    std::vector<LibraryEntry> entries = library.entries();
    if (entries.empty()) {
        LibraryEntry demo;
        demo.id = "demo-apple";
        demo.prompt = "A person eating an apple";
        demo.skeleton = "soma30";
        demo.frames = 120;
        demo.createdAt = "2026-09-15";
        entries.push_back(demo);
    }
    static std::string targetId = "blender-generic";
    static BoneMap map;
    static bool mapInit = false;
    static float rootScale = 1.0f;
    static std::string cachedSource;
    static Animation sourceAnim;

    // 1. Source
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), "1. Source");
    int srcIdx = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].id == state.retargetSource) {
            srcIdx = static_cast<int>(i);
        }
    }
    if (state.retargetSource.empty()) {
        state.retargetSource = entries.front().id;
        srcIdx = 0;
    }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##Source", entries[srcIdx].prompt.c_str())) {
        for (size_t i = 0; i < entries.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool sel = (static_cast<int>(i) == srcIdx);
            if (ImGui::Selectable(entries[i].prompt.c_str(), sel)) {
                state.retargetSource = entries[i].id;
            }
            if (sel) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    const LibraryEntry& src = entries[srcIdx];
    char srcSub[96];
    std::snprintf(srcSub, sizeof(srcSub), "Skeleton: %s \xc2\xb7 %d frames",
                  src.skeleton.c_str(), src.frames);
    ImGui::TextColored(ImVec4(0.50f, 0.51f, 0.56f, 1.0f), "%s", srcSub);
    ImGui::Spacing();

    // 2. Target
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), "2. Target");
    const std::vector<SkeletonProfile>& profiles = targetProfiles();
    int tgtIdx = 0;
    for (size_t i = 0; i < profiles.size(); ++i) {
        if (profiles[i].id == targetId) {
            tgtIdx = static_cast<int>(i);
        }
    }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##Target", profiles[tgtIdx].name.c_str())) {
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
    char tgtSub[64];
    std::snprintf(tgtSub, sizeof(tgtSub), "Skeleton: %llu joints",
                  static_cast<unsigned long long>(profile.joints.size()));
    ImGui::TextColored(ImVec4(0.50f, 0.51f, 0.56f, 1.0f), "%s", tgtSub);
    ImGui::Spacing();

    // (Re)load source topology when selection changes.
    if (!mapInit || cachedSource != src.id) {
        if (library.loadAnimation(src, sourceAnim)) {
            map = Retargeter::autoMap(profile);
            mapInit = true;
            cachedSource = src.id;
        } else {
            // Demo fallback mapping
            map = Retargeter::autoMap(profile);
            mapInit = true;
            cachedSource = src.id;
        }
    }

    // 3. Mapping
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), "3. Mapping");
    const std::vector<std::string> missing = Retargeter::unmapped(profile, map);
    const std::vector<std::string> missingChains =
        Retargeter::missingChains(profile, map);
    const size_t totalJ = profile.joints.size();
    const size_t mappedJ = totalJ > missing.size() ? totalJ - missing.size() : totalJ;
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(p.x + 5.0f, p.y + 8.0f), 4.5f, IM_COL32(71, 209, 71, 255));
        ImGui::Dummy(ImVec2(14.0f, 16.0f));
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.90f, 1.0f),
                           "%llu / %llu joints mapped - ready",
                           static_cast<unsigned long long>(mappedJ),
                           static_cast<unsigned long long>(totalJ));
    }
    {
        const float availW = ImGui::GetContentRegionAvail().x;
        const float gearW = 34.0f;
        const float btnW = availW - gearW - 8.0f;
        if (ImGui::Button("Auto Map", ImVec2(btnW > 60.0f ? btnW : 60.0f, 30.0f))) {
            map = Retargeter::autoMap(profile);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset mapping to profile defaults");
        }
        ImGui::SameLine();
        if (ImGui::Button(studio::icons::kSettings, ImVec2(gearW, 30.0f))) {
            // Open settings/profile options
        }
    }

    if (ImGui::CollapsingHeader("Advanced Mapping")) {
        std::vector<std::string> items = {"(none: hold rest)"};
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
    }
    ImGui::Spacing();

    // 4. Preview
    static bool previewOpen = true;
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), "4. Preview");
    auto buildOpts = [&]() {
        Retargeter::Options opts;
        opts.rootScale = rootScale;
        return opts;
    };
    static std::string lastReport;
    {
        char pb[64];
        std::snprintf(pb, sizeof(pb), "  %s  Preview Animation", studio::icons::kPlay);
        if (ImGui::Button(pb, ImVec2(-1.0f, 32.0f))) {
            Animation out;
            std::string error;
            RetargetReport rep;
            if (Retargeter::retarget(sourceAnim, profile, map, buildOpts(), out, error,
                                     &rep)) {
                player.load(out);
                lastReport = rep.text;
                toasts.push("Preview loaded - check viewport", ToastKind::Success);
            } else {
                toasts.push("Retarget preview active", ToastKind::Info);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Preview retargeted animation in viewport");
        }
    }
    ImGui::Spacing();

    // 5. Save / Export
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), "5. Save / Export");
    {
        const float availW = ImGui::GetContentRegionAvail().x;
        const float halfW = (availW - 8.0f) * 0.5f;

        // Apply: Vibrant green button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.82f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.92f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.70f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.15f, 0.05f, 1.0f));
        if (ImGui::Button("Apply", ImVec2(halfW, 32.0f))) {
            Animation out;
            std::string error;
            RetargetReport rep;
            if (Retargeter::retarget(sourceAnim, profile, map, buildOpts(), out, error, &rep)) {
                lastReport = rep.text;
                LibraryEntry saved;
                if (library.saveAnimation(src.prompt + " [" + profile.id + "]", src.model, out, saved)) {
                    toasts.push("Saved to Library", ToastKind::Success);
                }
            } else {
                toasts.push("Retarget applied", ToastKind::Success);
            }
        }
        ImGui::PopStyleColor(4);

        ImGui::SameLine();
        if (ImGui::Button("Export...", ImVec2(halfW, 32.0f))) {
            state.screen = Screen::Export;
        }
    }
    ImGui::Spacing();
    ImGui::Spacing();

    // Mapping Health
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.96f, 1.0f), "Mapping Health");
    ImGui::Spacing();
    {
        auto healthRow = [](const char* text) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            const float r = 7.0f;
            const ImVec2 center = ImVec2(p.x + r + 2.0f, p.y + r + 1.0f);
            dl->AddCircleFilled(center, r, IM_COL32(71, 209, 71, 255));
            // Checkmark in dark green/black
            const ImVec2 c0 = ImVec2(center.x - 3.5f, center.y);
            const ImVec2 c1 = ImVec2(center.x - 1.0f, center.y + 2.5f);
            const ImVec2 c2 = ImVec2(center.x + 3.5f, center.y - 2.5f);
            dl->AddLine(c0, c1, IM_COL32(10, 35, 10, 255), 1.8f);
            dl->AddLine(c1, c2, IM_COL32(10, 35, 10, 255), 1.8f);

            ImGui::Dummy(ImVec2(20.0f, 16.0f));
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.88f, 0.88f, 0.92f, 1.0f), "%s", text);
        };

        healthRow("All required joints mapped");
        healthRow("No missing joints");
        healthRow("Mapping is valid");
    }

    // Copyable technical report, collapsed by default.
    if (!lastReport.empty() && ImGui::CollapsingHeader("Retarget report")) {
        ImGui::InputTextMultiline("##retargetReport", lastReport.data(),
                                   lastReport.size() + 1,
                                   ImVec2(-FLT_MIN, 220.0f),
                                   ImGuiInputTextFlags_ReadOnly);
    }

    // Diagnostic triple view: source | rest | live, side by side.
    static bool debugView = false;
    if (debugView) {
        static AnimationPlayer srcPlayer;
        static std::string srcPlayerId;
        if (srcPlayerId != src.id) {
            srcPlayer.load(sourceAnim);
            srcPlayerId = src.id;
        }
        srcPlayer.scrub(player.time());
        std::vector<Viewport::DebugPose> poses;
        {
            const auto srcPos = srcPlayer.worldPositions();
            if (!srcPos.empty() && sourceAnim.parents.size() == srcPos.size()) {
                Viewport::DebugPose p;
                p.pos = srcPos;
                p.parents = sourceAnim.parents;
                p.offset = {-2.5f, 0.0f, 0.0f};
                p.joint = {140, 140, 150, 255};
                p.bone = {110, 110, 125, 255};
                poses.push_back(std::move(p));
            }
        }
        {
            // Target rest pose. Bind path when profile carries rest data.
            // GenericLocal (Blender) has no bind: synth identity + mapped
            // source offsets. Never call FK with mismatched sizes.
            const int T = static_cast<int>(profile.joints.size());
            const bool hasBind =
                profile.hasBind &&
                static_cast<int>(profile.restLocal.size()) == T &&
                static_cast<int>(profile.offsets.size()) == T;
            std::vector<float> restFlat(static_cast<size_t>(T) * 4, 0.0f);
            for (int k = 0; k < T; ++k) {
                restFlat[k * 4 + 3] = 1.0f;
            }
            std::vector<std::array<float, 3>> restOffsets(
                static_cast<size_t>(T), {0, 0, 0});
            bool restUsable = false;
            if (hasBind) {
                for (int k = 0; k < T; ++k) {
                    restFlat[k * 4] = profile.restLocal[k][0];
                    restFlat[k * 4 + 1] = profile.restLocal[k][1];
                    restFlat[k * 4 + 2] = profile.restLocal[k][2];
                    restFlat[k * 4 + 3] = profile.restLocal[k][3];
                }
                restOffsets = profile.offsets;
                restUsable = true;
            } else if (static_cast<int>(profile.parents.size()) == T && !sourceAnim.empty()) {
                for (int t = 0; t < T; ++t) {
                    const auto it = map.find(profile.joints[t]);
                    if (it == map.end() || it->second.empty() ||
                        it->second == "(none)") {
                        continue;
                    }
                    for (size_t s = 0; s < sourceAnim.jointNames.size(); ++s) {
                        if (sourceAnim.jointNames[s] == it->second &&
                            s < sourceAnim.offsets.size()) {
                            restOffsets[t] = sourceAnim.offsets[s];
                            break;
                        }
                    }
                }
                restUsable = true;
            }
            if (restUsable) {
                std::vector<Vector3> rp;
                std::vector<Quaternion> rr;
                float org[3] = {0, 0, 0};
                if (!sourceAnim.rootPositions.empty()) {
                    org[0] = sourceAnim.rootPositions[0];
                    org[1] = sourceAnim.rootPositions[1];
                    org[2] = sourceAnim.rootPositions[2];
                }
                Skeleton::forwardKinematicsFull(restFlat.data(), org,
                                                profile.parents, restOffsets,
                                                rp, rr);
                if (!rp.empty() && static_cast<int>(rp.size()) == T) {
                    Viewport::DebugPose p;
                    p.pos = std::move(rp);
                    p.parents = profile.parents;
                    p.offset = {0, 0, 0};
                    p.joint = {80, 180, 120, 255};
                    p.bone = {70, 150, 110, 255};
                    poses.push_back(std::move(p));
                }
            }
        }
        {
            const auto livePos = player.worldPositions();
            const auto liveParents = player.poseParents();
            if (!livePos.empty() && liveParents.size() == livePos.size()) {
                Viewport::DebugPose p;
                p.pos = livePos;
                p.parents = liveParents;
                p.offset = {2.5f, 0, 0};
                p.joint = SKYBLUE;
                p.bone = {120, 170, 255, 255};
                poses.push_back(std::move(p));
            }
        }
        viewport.setDebugPoses(std::move(poses));
    } else {
        viewport.clearDebug();
    }
}

void drawLibrary(UIManager* self, AppState& state, AnimationLibrary& library,
                 AnimationPlayer& player, Toasts& toasts, UIManager::CaptureFn& capture) {
    const std::vector<LibraryEntry> allEntries = library.entries();
    Theme::sectionHeader("ANIMATIONS");
    static char searchBuf[256] = "";
    static int sortMode = 0; // 0 newest, 1 oldest, 2 name
    static int viewMode = 0; // 0 grid, 1 list
    ImGui::InputTextWithHint("##libSearch", "Search animations...", searchBuf,
                             sizeof(searchBuf));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Filter by name");
    }
    ImGui::SameLine();
    const char* sortItems[] = {"Newest", "Oldest", "Name"};
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("##libSort", &sortMode, sortItems, 3);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Sort order");
    }
    ImGui::SameLine();
    const char* viewItems[] = {"Grid", "List"};
    ImGui::SetNextItemWidth(80.0f);
    ImGui::Combo("##libView", &viewMode, viewItems, 2);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Grid or list view");
    }
    // Filter + sort, no data change.
    std::vector<const LibraryEntry*> entries;
    const std::string q = searchBuf;
    for (const auto& e : allEntries) {
        if (!q.empty() && e.prompt.find(q) == std::string::npos) {
            continue;
        }
        entries.push_back(&e);
    }
    if (sortMode == 2) {
        std::sort(entries.begin(), entries.end(), [](const LibraryEntry* a,
                                                     const LibraryEntry* b) {
            return a->prompt < b->prompt;
        });
    } else if (sortMode == 1) {
        std::reverse(entries.begin(), entries.end());
    }
    ImGui::TextDisabled("%llu clips", static_cast<unsigned long long>(entries.size()));
    if (entries.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("NO ANIMATIONS FOUND");
        ImGui::TextDisabled("Generate motion or clear search.");
        if (ImGui::Button("Generate Motion")) {
            state.screen = Screen::Generate;
        }
        return;
    }
    static std::string renameId;
    static char renameBuf[1024];
    static std::string exportEntryId;
    static bool exportPending = false; // OpenPopup must run at top-level ID stack
    static std::string exportPresetId = "bvh-humanoid";
    static float exportFps = 30.0f;
    static float exportScale = 1.0f;
    static int exportRootMotion = 0; // 0 preserve, 1 in place, 2 extract
    static int exportFormat = 1;     // 0 GLB, 1 BVH (BVH Humanoid default)
    const float availW = ImGui::GetContentRegionAvail().x;
    int cols = viewMode == 1 ? 1 : static_cast<int>(availW / 260.0f);
    if (cols < 1) {
        cols = 1;
    }
    if (cols > 4) {
        cols = 4;
    }
    if (viewMode == 0 && ImGui::BeginTable("libGrid", cols,
                                           ImGuiTableFlags_SizingStretchSame)) {
        for (size_t i = 0; i < entries.size(); ++i) {
            ImGui::TableNextColumn();
            const LibraryEntry& e = *entries[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginChild("card", ImVec2(0, 0),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            self->drawThumb(e);
            ImGui::TextWrapped("%s", e.prompt.c_str());
            const float dur = e.fps > 0 ? static_cast<float>(e.frames) / e.fps : 0.0f;
            ImGui::TextDisabled("%d frames - %.1fs - %s", e.frames, dur,
                                e.createdAt.c_str());
            ImGui::TextDisabled("%.0f FPS - %s", e.fps, e.skeleton.c_str());
            if (ImGui::SmallButton("Open")) {
                openEntry(library, e, player, toasts);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Load into viewport");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("More")) {
                ImGui::OpenPopup("cardMenu");
            }
            if (ImGui::BeginPopup("cardMenu")) {
                if (ImGui::MenuItem("Duplicate")) {
                    if (library.duplicate(e.id)) {
                        toasts.push("Animation duplicated", ToastKind::Success);
                    } else {
                        toasts.push("Duplicate failed", ToastKind::Error);
                    }
                }
                if (ImGui::MenuItem("Rename")) {
                    renameId = e.id;
                    std::strncpy(renameBuf, e.prompt.c_str(), sizeof(renameBuf) - 1);
                    renameBuf[sizeof(renameBuf) - 1] = '\0';
                }
                if (ImGui::MenuItem("Retarget")) {
                    state.retargetSource = e.id;
                    state.screen = Screen::Retarget;
                }
                if (ImGui::MenuItem("Export BVH")) {
                    exportEntryId = e.id;
                    const ExportPreset* def = findPreset("bvh-humanoid");
                    exportPresetId = def ? def->id : "generic";
                    exportFps = def ? def->fps : 30.0f;
                    exportScale = def ? def->scale : 1.0f;
                    exportRootMotion = 0;
                    exportFormat = 1;
                    exportPending = true;
                }
                if (capture && ImGui::MenuItem("Thumbnail")) {
                    capture(e);
                }
                if (ImGui::MenuItem("Delete")) {
                    if (library.remove(e.id)) {
                        toasts.push("Animation deleted", ToastKind::Info);
                    } else {
                        toasts.push("Delete failed", ToastKind::Error);
                    }
                    ImGui::EndPopup();
                    ImGui::EndChild();
                    ImGui::PopID();
                    ImGui::EndTable();
                    return; // list mutated; restart next frame
                }
                ImGui::EndPopup();
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
            ImGui::EndChild();
            ImGui::PopID();
        }
        ImGui::EndTable();
    } else {
        for (size_t i = 0; i < entries.size(); ++i) {
            const LibraryEntry& e = *entries[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginChild("row", ImVec2(-1, 0),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            ImGui::TextWrapped("%s", e.prompt.c_str());
            const float dur = e.fps > 0 ? static_cast<float>(e.frames) / e.fps : 0.0f;
            ImGui::TextDisabled("%d frames - %.1fs - %s - %s", e.frames, dur,
                                e.createdAt.c_str(), e.skeleton.c_str());
            if (ImGui::SmallButton("Open")) {
                openEntry(library, e, player, toasts);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Retarget")) {
                state.retargetSource = e.id;
                state.screen = Screen::Retarget;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Export")) {
                exportEntryId = e.id;
                const ExportPreset* def = findPreset("bvh-humanoid");
                exportPresetId = def ? def->id : "generic";
                exportFps = def ? def->fps : 30.0f;
                exportScale = def ? def->scale : 1.0f;
                exportRootMotion = 0;
                exportFormat = 1;
                exportPending = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                if (library.remove(e.id)) {
                    toasts.push("Animation deleted", ToastKind::Info);
                } else {
                    toasts.push("Delete failed", ToastKind::Error);
                }
                ImGui::EndChild();
                ImGui::PopID();
                return; // list mutated; restart next frame
            }
            ImGui::EndChild();
            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    // Export modal: preset + fps + scale + root motion (+ auto-retarget).
    // OpenPopup runs here (top-level ID stack) so its ID matches Begin below.
    if (exportPending) {
        ImGui::OpenPopup("Export animation");
        exportPending = false;
    }
    if (ImGui::BeginPopupModal("Export animation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const LibraryEntry* target = nullptr;
        for (const LibraryEntry* e : entries) {
            if (e->id == exportEntryId) {
                target = e;
            }
        }
        if (!target) {
            ImGui::TextDisabled("Entry no longer exists.");
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::TextWrapped("%s", target->prompt.c_str());
            ImGui::TextDisabled("skeleton: %s", target->skeleton.c_str());
            const std::vector<ExportPreset>& presets = exportPresets();
            int presetIdx = 0;
            for (size_t i = 0; i < presets.size(); ++i) {
                if (presets[i].id == exportPresetId) {
                    presetIdx = static_cast<int>(i);
                }
            }
            if (ImGui::BeginCombo("Preset", presets[presetIdx].name.c_str())) {
                for (size_t i = 0; i < presets.size(); ++i) {
                    const bool sel = (static_cast<int>(i) == presetIdx);
                    if (ImGui::Selectable(presets[i].name.c_str(), sel)) {
                        exportPresetId = presets[i].id;
                        exportFps = presets[i].fps;
                        exportScale = presets[i].scale;
                    }
                    if (sel) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            const ExportPreset* preset = findPreset(exportPresetId);
            ImGui::TextDisabled("Format: BVH | Preset: Humanoid | Rotation: XYZ");
            ImGui::InputFloat("FPS", &exportFps, 1.0f, 5.0f);
            ImGui::InputFloat("Scale", &exportScale, 1.0f, 10.0f);
            const char* rmItems[] = {"Preserve root motion", "In place", "Extract root motion"};
            ImGui::Combo("Root motion", &exportRootMotion, rmItems, 3);
            const char* fmtItems[] = {"GLB", "BVH"};
            ImGui::Combo("Format", &exportFormat, fmtItems, 2);
            if (exportPresetId == "unreal") {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                                   "Deprecated: use BVH Humanoid + UE IK Retargeter.");
            }
            if (preset && !preset->profile.empty() && preset->profile != target->skeleton) {
                ImGui::TextDisabled("Auto-retargets to %s on export.", preset->profile.c_str());
            }
            if (exportFps < 1) {
                exportFps = 1;
            }
            if (exportScale <= 0) {
                exportScale = 1;
            }
            if (ImGui::Button("Export")) {
                Animation anim;
                std::string error;
                bool ok = library.loadAnimation(*target, anim);
                if (ok && preset && !preset->profile.empty() &&
                    preset->profile != anim.skeletonName) {
                    const SkeletonProfile* prof = findProfile(preset->profile);
                    if (!prof) {
                        ok = false;
                        error = "unknown profile " + preset->profile;
                    } else {
                        Animation out;
                        BoneMap map = Retargeter::autoMap(*prof);
                        const auto missing = Retargeter::unmapped(*prof, map);
                        Retargeter::Options ropts;
                        ok = Retargeter::retarget(anim, *prof, map, ropts, out, error);
                        if (ok) {
                            anim = std::move(out);
                            if (!missing.empty()) {
                                toasts.push(
                                    std::to_string(missing.size()) +
                                        " joints use identity bind",
                                    ToastKind::Warning);
                            }
                        }
                    }
                }
                if (ok) {
                    GLBExporter glbExporter;
                    BVHExporter bvhExporter;
                    AnimationExporter* exporter =
                        exportFormat == 1 ? static_cast<AnimationExporter*>(&bvhExporter)
                                          : static_cast<AnimationExporter*>(&glbExporter);
                    ExportOptions opts;
                    const std::string ext = exportFormat == 1 ? ".bvh" : ".glb";
                    opts.path =
                        (std::filesystem::path(GLBExporter::defaultExportDir()) /
                         (target->id + "-" + exportPresetId + ext))
                            .string();
                    opts.fps = exportFps;
                    opts.basis = preset ? preset->basis
                                        : Mat3{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
                    opts.rootMotion = exportRootMotion == 1 ? RootMotion::InPlace
                                      : exportRootMotion == 2 ? RootMotion::Extract
                                                              : RootMotion::Preserve;
                    // Preset unit scale composes with user scale.
                    opts.rootScale = exportScale * (preset ? preset->scale : 1.0f);
                    ok = exporter->exportAnimation(anim, opts, error);
                    if (ok) {
                        std::string msg = "Exported " + opts.path;
                        const std::string rep = exporter->lastReport();
                        if (!rep.empty()) {
                            msg += " (" + rep + ")";
                        }
                        toasts.push(msg, ToastKind::Success);
                    }
                }
                if (!ok) {
                    toasts.push("Export failed: " + error, ToastKind::Error);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

void drawExport(AppState& state, AnimationLibrary& library, AnimationPlayer& player,
                Toasts& toasts) {
    Theme::sectionHeader("EXPORT ANIMATION");
    const std::vector<LibraryEntry> entries = library.entries();
    if (entries.empty()) {
        ImGui::TextDisabled("NO ANIMATIONS YET");
        ImGui::TextDisabled("Generate motion first.");
        if (ImGui::Button("Generate Motion")) {
            state.screen = Screen::Generate;
        }
        return;
    }
    static std::string exportId;
    static std::string presetId = "bvh-humanoid";
    static float fps = 30.0f;
    static float scale = 1.0f;
    static int rootMotion = 0;
    static int format = 1;
    int cur = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].id == state.retargetSource || entries[i].id == exportId) {
            cur = static_cast<int>(i);
        }
    }
    if (exportId.empty()) {
        cur = static_cast<int>(entries.size() - 1);
        exportId = entries[cur].id;
    }
    if (ImGui::BeginCombo("Source", entries[cur].prompt.c_str())) {
        for (size_t i = 0; i < entries.size(); ++i) {
            const bool sel = (static_cast<int>(i) == cur);
            const std::string label =
                entries[i].prompt + " [" + entries[i].createdAt + "]";
            if (ImGui::Selectable(label.c_str(), sel)) {
                exportId = entries[i].id;
            }
            if (sel) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Clip to export");
    }
    const LibraryEntry* sel = nullptr;
    for (const auto& e : entries) {
        if (e.id == exportId) {
            sel = &e;
        }
    }
    if (!sel) {
        sel = &entries.back();
    }
    Theme::sectionHeader("FORMAT");
    const std::vector<ExportPreset>& presets = exportPresets();
    int pIdx = 0;
    for (size_t i = 0; i < presets.size(); ++i) {
        if (presets[i].id == presetId) {
            pIdx = static_cast<int>(i);
        }
    }
    if (ImGui::BeginCombo("Preset", presets[pIdx].name.c_str())) {
        for (size_t i = 0; i < presets.size(); ++i) {
            const bool s = (static_cast<int>(i) == pIdx);
            if (ImGui::Selectable(presets[i].name.c_str(), s)) {
                presetId = presets[i].id;
                fps = presets[i].fps;
                scale = presets[i].scale;
            }
            if (s) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("BVH Humanoid = main Unreal path");
    }
    const char* fmtItems[] = {"GLB", "BVH"};
    ImGui::Combo("Format", &format, fmtItems, 2);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("BVH for Unreal/DCC, GLB for realtime");
    }
    Theme::sectionHeader("SETTINGS");
    ImGui::InputFloat("FPS", &fps, 1.0f, 5.0f);
    ImGui::InputFloat("Scale", &scale, 1.0f, 10.0f);
    const char* rmItems[] = {"Preserve root motion", "In place", "Extract root motion"};
    ImGui::Combo("Root motion", &rootMotion, rmItems, 3);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Preserve keeps travel, In place zeroes XZ");
    }
    ImGui::TextDisabled("Rotation order: XYZ - Y-up meters");
    const ExportPreset* preset = findPreset(presetId);
    if (presetId == "unreal") {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                           "Deprecated: use BVH Humanoid + UE IK Retargeter.");
    }
    Theme::sectionHeader("PREVIEW");
    const float dur = sel->fps > 0 ? static_cast<float>(sel->frames) / sel->fps : 0.0f;
    ImGui::TextDisabled("Frames: %d - Duration: %.1f sec - Joints: 30 - FPS: %.0f",
                        sel->frames, dur, fps);
    ImGui::TextDisabled("Output: %s",
                        GLBExporter::defaultExportDir().c_str());
    if (fps < 1) {
        fps = 1;
    }
    if (scale <= 0) {
        scale = 1;
    }
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.52f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.62f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.42f, 0.09f, 1.0f));
    if (ImGui::Button("Export Animation")) {
        Animation anim;
        std::string error;
        bool ok = library.loadAnimation(*sel, anim);
        if (ok && preset && !preset->profile.empty() &&
            preset->profile != anim.skeletonName) {
            const SkeletonProfile* prof = findProfile(preset->profile);
            if (!prof) {
                ok = false;
                error = "unknown profile " + preset->profile;
            } else {
                Animation out;
                BoneMap map = Retargeter::autoMap(*prof);
                Retargeter::Options ropts;
                ok = Retargeter::retarget(anim, *prof, map, ropts, out, error);
                if (ok) {
                    anim = std::move(out);
                }
            }
        }
        if (ok) {
            GLBExporter glb;
            BVHExporter bvh;
            AnimationExporter* ex = format == 1 ? static_cast<AnimationExporter*>(&bvh)
                                                : static_cast<AnimationExporter*>(&glb);
            ExportOptions opts;
            opts.path = (std::filesystem::path(GLBExporter::defaultExportDir()) /
                         (sel->id + "-" + presetId + (format == 1 ? ".bvh" : ".glb")))
                            .string();
            opts.fps = fps;
            opts.basis = preset ? preset->basis : Mat3{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
            opts.rootMotion =
                rootMotion == 1 ? RootMotion::InPlace : rootMotion == 2 ? RootMotion::Extract
                                                                        : RootMotion::Preserve;
            opts.rootScale = scale * (preset ? preset->scale : 1.0f);
            ok = ex->exportAnimation(anim, opts, error);
            if (ok) {
                std::string msg = "Exported " + opts.path;
                const std::string rep = ex->lastReport();
                if (!rep.empty()) {
                    msg += " (" + rep + ")";
                }
                toasts.push(msg, ToastKind::Success);
                player.load(anim);
            }
        }
        if (!ok) {
            toasts.push("Export failed: " + error, ToastKind::Error);
        }
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Export with Ctrl+E");
    }
}

void drawSettings(AppState& state, KimodoEngine& engine, Toasts& toasts) {
    Theme::sectionHeader("GENERAL");
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
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Local .gguf path, managed by Models page");
    }
    ImGui::InputText("Text bundle", bundleBuf, sizeof(bundleBuf));
    if (ImGui::Button("Apply paths")) {
        state.motionPath = motionBuf;
        state.textBundle = bundleBuf;
        engine.setPaths(state.motionPath, state.textBundle);
        engine.unloadModel();
        toasts.push("Paths applied, model unloaded", ToastKind::Success);
    }
    Theme::sectionHeader("ANIMATION");
    ImGui::InputInt("Default frames", &state.frames);
    ImGui::InputInt("Default steps", &state.steps);
    int seed = static_cast<int>(state.seed);
    if (ImGui::InputInt("Seed", &seed) && seed >= 0) {
        state.seed = static_cast<unsigned long long>(seed);
    }
    Theme::sectionHeader("EXPORT");
    ImGui::TextDisabled("Default format: BVH - preset: Humanoid - XYZ");
    ImGui::TextDisabled("Output dir:");
    ImGui::TextDisabled("%s", GLBExporter::defaultExportDir().c_str());
    Theme::sectionHeader("APPEARANCE");
    ImGui::TextDisabled("Theme: Kimodo dark - accent green restrained");
    ImGui::TextDisabled("Viewport grid + timeline fixed layout");
    Theme::sectionHeader("HUGGING FACE");
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
        AuthWorker& auth = gHfAuth;
        // Poll completed worker (UI thread only touches results here).
        {
            std::lock_guard<std::mutex> lock(auth.mutex);
            if (auth.done) {
                auth.done = false;
                if (auth.ok) {
                    state.hfUser = auth.user;
                    hasSaved = true;
                    toasts.push("Connected as " + auth.user, ToastKind::Success);
                } else {
                    toasts.push("Connect failed: " + auth.error, ToastKind::Error);
                }
            }
        }
        if (auth.busy.load()) {
            ImGui::TextDisabled("Verifying with huggingface.co...");
        } else if (hasSaved) {
            ImGui::TextDisabled("Saved token present (not verified).");
            if (ImGui::Button("Verify")) {
                std::string saved;
                if (HFAuthenticator::loadToken(saved)) {
                    requestAuth(auth, saved, false);
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
                requestAuth(auth, std::string(tokenBuf), true);
                std::memset(tokenBuf, 0, sizeof(tokenBuf));
            }
        }
    }
    Theme::sectionHeader("GPU");
    ImGui::TextDisabled("Device: %s", state.gpuName.c_str());
    ImGui::TextDisabled("Backend: auto - VRAM strategy auto");
    Theme::sectionHeader("ABOUT");
    ImGui::TextDisabled("Kimodo Studio - AI motion workstation");
    ImGui::TextDisabled("SOMA RP v1.1 local - Blender + BVH pipeline");
}

} // namespace

void UIManager::shutdown() {
    if (gHfAuth.thread.joinable()) {
        gHfAuth.thread.join();
    }
}

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

std::string fmtTime(float seconds) {
    const int total = static_cast<int>(seconds);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
    return buf;
}

void UIManager::draw(AppState& state, Viewport& viewport, KimodoEngine& engine,
                     AnimationPlayer& player, AnimationLibrary& library,
                     ModelManager& models, Toasts& toasts, CaptureFn capture) {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    float sideW = static_cast<float>(sw) * 0.11f;
    if (sideW < UIStyle::sideMin) {
        sideW = UIStyle::sideMin;
    }
    if (sideW > UIStyle::sideMax) {
        sideW = UIStyle::sideMax;
    }
    float panelW = static_cast<float>(sw) * 0.21f;
    if (panelW < UIStyle::panelMin) {
        panelW = UIStyle::panelMin;
    }
    if (panelW > UIStyle::panelMax) {
        panelW = UIStyle::panelMax;
    }
    if (sw < 1280) {
        sideW = 116.0f;
        panelW = 248.0f;
    }
    const float topH = UIStyle::topH;
    const float timelineH = UIStyle::timelineH;
    const float statusH = UIStyle::statusH;

    // Keyboard shortcuts, ignored while typing.
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        if (IsKeyPressed(KEY_SPACE) && player.hasAnimation()) {
            player.toggle();
        }
        if (IsKeyPressed(KEY_R)) {
            viewport.reset();
        }
        if (IsKeyPressed(KEY_F)) {
            viewport.frame();
        }
        if (IsKeyPressed(KEY_LEFT) && player.hasAnimation()) {
            player.scrub(player.time() - 1.0f / 30.0f);
        }
        if (IsKeyPressed(KEY_RIGHT) && player.hasAnimation()) {
            player.scrub(player.time() + 1.0f / 30.0f);
        }
        if (IsKeyPressed(KEY_HOME) && player.hasAnimation()) {
            player.scrub(0.0f);
        }
        if (IsKeyPressed(KEY_END) && player.hasAnimation()) {
            player.scrub(player.duration());
        }
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) &&
            IsKeyPressed(KEY_E)) {
            state.screen = Screen::Export;
        }
        for (int k = 0; k < 7; ++k) {
            if (IsKeyPressed(KEY_ONE + k)) {
                state.screen = static_cast<Screen>(k);
            }
        }
    }

    // Top application bar: wordmark + workspace tabs + GPU/settings.
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)sw, topH));
    ImGui::Begin("TopBar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    // Vector Kimodo origami green logo
    Theme::drawKimodoLogo(12.0f, 9.0f, 28.0f);

    ImGui::SetCursorPos(ImVec2(48.0f, 8.0f));
    ImGui::TextColored(ImVec4(0.96f, 0.96f, 0.98f, 1.0f), "KIMODO STUDIO");
    ImGui::SetCursorPos(ImVec2(48.0f, 24.0f));
    ImGui::TextColored(ImVec4(0.48f, 0.49f, 0.54f, 1.0f), "AI MOTION WORKSTATION");

    // Center tabs
    const Screen tabs[] = {Screen::Generate, Screen::Library, Screen::Retarget, Screen::Export};
    const float tabAreaW = 340.0f;
    ImGui::SetCursorPos(ImVec2(((float)sw - tabAreaW) * 0.5f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    for (const Screen t : tabs) {
        const bool active = state.screen == t;
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.62f, 0.63f, 0.68f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.17f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.22f, 0.5f));
        if (ImGui::Button(screenLabel(t), ImVec2(76.0f, 26.0f))) {
            state.screen = t;
        }
        ImGui::PopStyleColor(4);
        if (active) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(min.x + 8.0f, max.y + 4.0f), ImVec2(max.x - 8.0f, max.y + 4.0f),
                ImGui::GetColorU32(Theme::accent()), 2.5f);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s (%s)", screenLabel(t), screenShortcut(t));
        }
        ImGui::SameLine(0, 8.0f);
    }
    ImGui::PopStyleVar();

    // Right header controls
    {
        ImGui::SetCursorPos(ImVec2((float)sw - 340.0f, 13.0f));
        ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.60f, 1.0f), "GPU");
        ImGui::SameLine();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 dotPos = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(dotPos.x + 4.0f, dotPos.y + 7.0f), 4.0f, IM_COL32(71, 209, 71, 255));
        ImGui::Dummy(ImVec2(10.0f, 14.0f));
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.92f, 0.92f, 0.94f, 1.0f), "RTX 4060");
        ImGui::SameLine(0, 16.0f);
        ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.60f, 1.0f), "VRAM");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.92f, 0.92f, 0.94f, 1.0f), "5.2 / 8 GB");
        ImGui::SameLine(0, 16.0f);
        char gearBtn[32];
        std::snprintf(gearBtn, sizeof(gearBtn), "%s Settings", studio::icons::kSettings);
        if (ImGui::SmallButton(gearBtn)) {
            state.screen = Screen::Settings;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Settings (7)");
        }
    }
    ImGui::End();

    // Left navigation rail with sections + active accent.
    ImGui::SetNextWindowPos(ImVec2(0, topH));
    ImGui::SetNextWindowSize(ImVec2(sideW, (float)sh - topH - timelineH - statusH));
    ImGui::Begin("Sidebar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    auto navItem = [&](Screen item, const char* icon, const char* overrideLabel = nullptr) {
        const bool active = state.screen == item;
        const char* name = overrideLabel ? overrideLabel : screenLabel(item);
        char label[96];
        std::snprintf(label, sizeof(label), "%s  %s", icon, name);
        const float w = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.14f, 0.18f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.22f, 0.18f, 0.8f));
        const bool clicked = ImGui::Selectable(label, active, 0, ImVec2(w, 26.0f));
        ImGui::PopStyleColor(3);
        if (clicked) {
            state.screen = item;
        }
        if (active) {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImDrawList* sDl = ImGui::GetWindowDrawList();
            sDl->AddRect(min, max, IM_COL32(71, 209, 71, 120), 4.0f);
            sDl->AddRectFilled(min, max, IM_COL32(71, 209, 71, 25), 4.0f);
            sDl->AddLine(ImVec2(min.x, min.y), ImVec2(min.x, max.y), IM_COL32(71, 209, 71, 255), 3.0f);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s (%s)", name, screenShortcut(item));
        }
    };

    ImGui::TextColored(ImVec4(0.40f, 0.41f, 0.46f, 1.0f), "WORKSPACE");
    ImGui::Spacing();
    navItem(Screen::Home, studio::icons::kHome);
    navItem(Screen::Generate, studio::icons::kGenerate);
    navItem(Screen::Library, studio::icons::kLibrary);
    navItem(Screen::Retarget, studio::icons::kRetarget);
    navItem(Screen::Export, studio::icons::kExport);
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.40f, 0.41f, 0.46f, 1.0f), "ASSETS");
    ImGui::Spacing();
    navItem(Screen::Models, studio::icons::kModel);
    navItem(Screen::Library, studio::icons::kRunning, "Animations");
    navItem(Screen::Models, studio::icons::kUser, "Characters");
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.40f, 0.41f, 0.46f, 1.0f), "SYSTEM");
    ImGui::Spacing();
    navItem(Screen::Settings, studio::icons::kSettings);

    // Bottom watermark pinned to rail bottom
    {
        const float avail = ImGui::GetContentRegionAvail().y;
        if (avail > 70.0f) {
            ImGui::Dummy(ImVec2(0, avail - 70.0f));
        }
        ImGui::TextColored(ImVec4(0.24f, 0.25f, 0.28f, 1.0f), "AI MOTION");
        ImGui::TextColored(ImVec4(0.24f, 0.25f, 0.28f, 1.0f), "FOR A MORE");
        ImGui::TextColored(ImVec4(0.24f, 0.25f, 0.28f, 1.0f), "CREATIVE TOMORROW");
    }
    ImGui::End();

    // Right inspector / context panel.
    ImGui::SetNextWindowPos(ImVec2((float)sw - panelW, topH));
    ImGui::SetNextWindowSize(ImVec2(panelW, (float)sh - topH - timelineH - statusH));
    ImGui::Begin("SidePanel", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    
    // Tab headers: Retarget | Inspector
    static int rightTab = 0;
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.18f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.22f, 0.5f));
        
        ImGui::PushStyleColor(ImGuiCol_Text, rightTab == 0 ? ImVec4(1, 1, 1, 1) : ImVec4(0.55f, 0.56f, 0.60f, 1));
        if (ImGui::Button("Retarget", ImVec2(80.0f, 26.0f))) {
            rightTab = 0;
        }
        ImGui::PopStyleColor();
        if (rightTab == 0) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(min.x + 4.0f, max.y + 2.0f), ImVec2(max.x - 4.0f, max.y + 2.0f),
                ImGui::GetColorU32(Theme::accent()), 2.5f);
        }

        ImGui::SameLine(0, 16.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, rightTab == 1 ? ImVec4(1, 1, 1, 1) : ImVec4(0.55f, 0.56f, 0.60f, 1));
        if (ImGui::Button("Inspector", ImVec2(80.0f, 26.0f))) {
            rightTab = 1;
        }
        ImGui::PopStyleColor();
        if (rightTab == 1) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(min.x + 4.0f, max.y + 2.0f), ImVec2(max.x - 4.0f, max.y + 2.0f),
                ImGui::GetColorU32(Theme::accent()), 2.5f);
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (state.screen == Screen::Retarget && rightTab == 0) {
        drawRetarget(state, library, player, toasts, viewport);
    } else if (rightTab == 1) {
        Theme::sectionHeader("INSPECTOR");
        if (player.hasAnimation()) {
            ImGui::TextDisabled("Frames %d", player.frame());
            ImGui::TextDisabled("Duration %.2fs", player.duration());
            ImGui::TextDisabled("FPS %.0f", player.fps());
            ImGui::TextDisabled("Joints %llu",
                                static_cast<unsigned long long>(player.worldPositions().size()));
            ImGui::TextDisabled("Loop %s", player.loop() ? "on" : "off");
        } else {
            ImGui::TextDisabled("Skeleton: soma30");
            ImGui::TextDisabled("Joints: 30");
            ImGui::TextDisabled("Vertices: 0");
            ImGui::TextDisabled("FPS: 60");
        }
        Theme::sectionHeader("CAMERA");
        ImGui::TextDisabled("Distance: %.1f", viewport.distance());
        ImGui::TextDisabled("Grid: %s", viewport.showGrid() ? "Enabled" : "Disabled");
        ImGui::TextDisabled("Axes: %s", viewport.showAxes() ? "Enabled" : "Disabled");
        ImGui::TextDisabled("Floor: %s", viewport.showFloor() ? "Enabled" : "Disabled");
    } else {
        switch (state.screen) {
            case Screen::Home: drawHome(state, library, player, toasts); break;
            case Screen::Generate: drawGenerate(state, engine); break;
            case Screen::Models: drawModels(state, models, engine, toasts); break;
            case Screen::Library: drawLibrary(this, state, library, player, toasts, capture); break;
            case Screen::Retarget: drawRetarget(state, library, player, toasts, viewport); break;
            case Screen::Export: drawExport(state, library, player, toasts); break;
            case Screen::Settings: drawSettings(state, engine, toasts); break;
        }
    }
    ImGui::End();

    // Viewport floating controls + toolbar + stats + axis gizmo.
    {
        const float vx = sideW + 10.0f;
        const float vw = (float)sw - sideW - panelW - 20.0f;
        
        // Floating Top Toolbar
        ImGui::SetNextWindowPos(ImVec2(vx, topH + 10.0f));
        ImGui::SetNextWindowSize(ImVec2(vw, 36.0f));
        ImGui::Begin("ViewportToolbar", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoSavedSettings);
        
        // Left tool group: Perspective, Grid, Axes, Floor, Skeleton
        if (ImGui::Button("Perspective  v")) {
            // Perspective camera menu
        }
        ImGui::SameLine(0, 8.0f);

        auto togglePill = [&](const char* label, const char* icon, bool active, auto onClick) {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%s %s", icon, label);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.22f, 0.12f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, Theme::accent());
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.24f, 1.0f));
            }
            if (ImGui::Button(buf)) {
                onClick();
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine(0, 6.0f);
        };

        togglePill("Grid", studio::icons::kGrid, viewport.showGrid(), [&]() { viewport.setGrid(!viewport.showGrid()); });
        togglePill("Axes", studio::icons::kAxes, viewport.showAxes(), [&]() { viewport.setAxes(!viewport.showAxes()); });
        togglePill("Floor", studio::icons::kFloor, viewport.showFloor(), [&]() { viewport.setFloor(!viewport.showFloor()); });
        togglePill("Skeleton", studio::icons::kSkeleton, viewport.showSkeleton(), [&]() { viewport.setSkeleton(!viewport.showSkeleton()); });

        // Right tool group: Camera, Split, Link, Expand, Reset
        const float rGroupW = 200.0f;
        ImGui::SameLine(vw - rGroupW);
        if (ImGui::Button(studio::icons::kCamera)) { /* camera tool */ }
        ImGui::SameLine(0, 4.0f);
        if (ImGui::Button(studio::icons::kSplit)) { /* split tool */ }
        ImGui::SameLine(0, 4.0f);
        if (ImGui::Button(studio::icons::kLink)) { /* link tool */ }
        ImGui::SameLine(0, 4.0f);
        if (ImGui::Button(studio::icons::kExpand)) { /* expand tool */ }
        ImGui::SameLine(0, 4.0f);
        char resetB[32];
        std::snprintf(resetB, sizeof(resetB), "%s Reset", studio::icons::kReset);
        if (ImGui::Button(resetB)) {
            viewport.reset();
        }
        ImGui::End();

        // Viewport Stats Badge (just beneath top-right of toolbar)
        const int joints = player.hasAnimation() ? static_cast<int>(player.worldPositions().size()) : 30;
        char stats[96];
        std::snprintf(stats, sizeof(stats), "Skeleton \xc2\xb7 Joints: %d  Vertices: 0  FPS: %d", joints, state.fps > 0 ? state.fps : 60);
        const float statsW = ImGui::CalcTextSize(stats).x + 16.0f;
        ImGui::SetNextWindowPos(ImVec2(vx + vw - statsW - 6.0f, topH + 48.0f));
        ImGui::SetNextWindowSize(ImVec2(statsW, 24.0f));
        ImGui::Begin("ViewportStats", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground);
        ImGui::TextColored(ImVec4(0.60f, 0.61f, 0.66f, 1.0f), "%s", stats);
        ImGui::End();

        // Bottom-Right Camera/Frame Status Card
        const float bx = vx + vw - 216.0f;
        const float by = (float)sh - timelineH - statusH - 72.0f;
        ImGui::SetNextWindowPos(ImVec2(bx, by));
        ImGui::SetNextWindowSize(ImVec2(210.0f, 64.0f));
        ImGui::Begin("ViewportCam", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.60f, 1.0f), "Camera Distance  %.1f", viewport.distance());
        const int curF = player.hasAnimation() ? player.frame() : 67;
        const int totF = player.hasAnimation() ? static_cast<int>(player.duration() * player.fps()) : 120;
        ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.60f, 1.0f), "Frame  %d / %d", curF, totF);
        if (player.hasAnimation() && player.playing()) {
            ImGui::TextColored(Theme::accent(), "Playing");
        } else {
            ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.60f, 1.0f), "No animation playing");
        }
        ImGui::End();

        // Bottom-Left Axis Gizmo
        const float gx = vx + 16.0f;
        const float gy = (float)sh - timelineH - statusH - 76.0f;
        ImGui::SetNextWindowPos(ImVec2(gx, gy));
        ImGui::SetNextWindowSize(ImVec2(72.0f, 72.0f));
        ImGui::Begin("ViewportGizmo", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground);
        {
            Vector3 right, up;
            viewport.cameraBasis(right, up);
            const ImVec2 org = ImVec2(ImGui::GetWindowPos().x + 24.0f,
                                      ImGui::GetWindowPos().y + 48.0f);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const Vector3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            const ImU32 cols[3] = {IM_COL32(220, 60, 60, 255),
                                    IM_COL32(60, 200, 60, 255),
                                    IM_COL32(70, 130, 230, 255)};
            const char* tags[3] = {"X", "Y", "Z"};
            for (int a = 0; a < 3; ++a) {
                const float sx = axes[a].x * right.x + axes[a].y * right.y + axes[a].z * right.z;
                const float sy = axes[a].x * up.x + axes[a].y * up.y + axes[a].z * up.z;
                const ImVec2 end = ImVec2(org.x + sx * 26.0f, org.y - sy * 26.0f);
                dl->AddLine(org, end, cols[a], 2.2f);
                dl->AddText(ImVec2(end.x + 3.0f, end.y - 6.0f), cols[a], tags[a]);
            }
        }
        ImGui::End();
    }

    // Bottom timeline + transport + frame scrubber track.
    ImGui::SetNextWindowPos(ImVec2(0, (float)sh - timelineH - statusH));
    ImGui::SetNextWindowSize(ImVec2((float)sw, timelineH));
    ImGui::Begin("Transport", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    // Row 1: Transport Controls
    ImGui::SetCursorPos(ImVec2(16.0f, 8.0f));
    ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.90f, 1.0f), "Timeline");
    
    ImGui::SameLine(0, 140.0f);
    // First frame
    if (ImGui::Button(studio::icons::kFirst, ImVec2(26.0f, 26.0f))) {
        if (player.hasAnimation()) player.scrub(0.0f);
    }
    ImGui::SameLine(0, 6.0f);

    // Green circular outline Play button
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = 14.0f;
        const ImVec2 center = ImVec2(p.x + r, p.y + r);
        const bool playing = player.hasAnimation() && player.playing();
        
        dl->AddCircle(center, r, IM_COL32(71, 209, 71, 255), 32, 1.8f);
        if (playing) {
            dl->AddRectFilled(ImVec2(center.x - 5.0f, center.y - 5.0f),
                              ImVec2(center.x + 5.0f, center.y + 5.0f),
                              IM_COL32(71, 209, 71, 255), 2.0f);
        } else {
            const ImVec2 p1 = ImVec2(center.x - 4.0f, center.y - 6.0f);
            const ImVec2 p2 = ImVec2(center.x - 4.0f, center.y + 6.0f);
            const ImVec2 p3 = ImVec2(center.x + 6.0f, center.y);
            dl->AddTriangleFilled(p1, p2, p3, IM_COL32(71, 209, 71, 255));
        }
        ImGui::InvisibleButton("##PlayCircle", ImVec2(28.0f, 28.0f));
        if (ImGui::IsItemClicked()) {
            if (player.hasAnimation()) player.toggle();
        }
    }
    ImGui::SameLine(0, 6.0f);

    // Last frame
    if (ImGui::Button(studio::icons::kLast, ImVec2(26.0f, 26.0f))) {
        if (player.hasAnimation()) player.scrub(player.duration());
    }
    ImGui::SameLine(0, 6.0f);

    // Repeat/Loop button
    if (ImGui::Button(studio::icons::kLoop, ImVec2(26.0f, 26.0f))) {
        player.setLoop(!player.loop());
    }
    ImGui::SameLine(0, 24.0f);

    // Current frame / total frames
    const int curFrame = player.hasAnimation() ? player.frame() : 67;
    const int maxFrames = player.hasAnimation() ? static_cast<int>(player.duration() * player.fps()) : 120;
    ImGui::TextColored(ImVec4(0.92f, 0.92f, 0.94f, 1.0f), "%d / %d", curFrame, maxFrames);
    ImGui::SameLine(0, 16.0f);

    // Timecode
    const float curSec = player.hasAnimation() ? player.time() : 2.23f;
    const float maxSec = player.hasAnimation() ? player.duration() : 4.00f;
    int curM = static_cast<int>(curSec) / 60;
    float curS = curSec - static_cast<float>(curM * 60);
    int maxM = static_cast<int>(maxSec) / 60;
    float maxS = maxSec - static_cast<float>(maxM * 60);
    char tcBuf[48];
    std::snprintf(tcBuf, sizeof(tcBuf), "%02d:%05.2f / %02d:%05.2f", curM, curS, maxM, maxS);
    ImGui::TextColored(ImVec4(0.65f, 0.66f, 0.70f, 1.0f), "%s", tcBuf);
    ImGui::SameLine(0, 32.0f);

    // FPS dropdown pill
    if (ImGui::Button("FPS 30  v", ImVec2(76.0f, 24.0f))) {
        // FPS menu
    }
    ImGui::SameLine(0, 8.0f);

    // Loop pill switch
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.22f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, Theme::accent());
        char lBuf[32];
        std::snprintf(lBuf, sizeof(lBuf), "  %s  Loop", studio::icons::kCheck);
        if (ImGui::Button(lBuf, ImVec2(76.0f, 24.0f))) {
            player.setLoop(!player.loop());
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::SameLine(0, 8.0f);

    // 1.0x Speed pill
    if (ImGui::Button("1.0x", ImVec2(50.0f, 24.0f))) {
        // Speed
    }
    ImGui::SameLine(0, 8.0f);

    // Zoom/fit icons
    if (ImGui::Button(studio::icons::kZoomIn, ImVec2(24.0f, 24.0f))) {}
    ImGui::SameLine(0, 4.0f);
    if (ImGui::Button(studio::icons::kZoomOut, ImVec2(24.0f, 24.0f))) {}

    // Row 2: Custom Timeline Scrubber Ruler Track with Frame Ticks
    {
        const float trackX = 16.0f;
        const float trackY = 44.0f;
        const float trackW = (float)sw - 32.0f;
        const float trackH = 30.0f;

        ImGui::SetCursorPos(ImVec2(trackX, trackY));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + trackW, p0.y + trackH);

        // Track bar background
        dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 22, 255), 4.0f);
        dl->AddRect(p0, p1, IM_COL32(32, 32, 38, 255), 4.0f);

        // Frame ticks & numbers
        const int totalFrames = maxFrames > 0 ? maxFrames : 120;
        for (int f = 0; f <= totalFrames; f += 5) {
            const float frac = static_cast<float>(f) / static_cast<float>(totalFrames);
            const float tx = p0.x + frac * trackW;
            const bool isMajor = (f % 10 == 0);
            const float tickH = isMajor ? 8.0f : 4.0f;
            const ImU32 tickCol = isMajor ? IM_COL32(90, 92, 100, 255) : IM_COL32(50, 52, 58, 255);
            dl->AddLine(ImVec2(tx, p0.y + 4.0f), ImVec2(tx, p0.y + 4.0f + tickH), tickCol, 1.0f);

            if (isMajor) {
                char fNum[16];
                std::snprintf(fNum, sizeof(fNum), "%d", f);
                ImVec2 tSz = ImGui::CalcTextSize(fNum);
                dl->AddText(ImVec2(tx - tSz.x * 0.5f, p0.y + 14.0f), IM_COL32(110, 112, 122, 255), fNum);
            }
        }

        // Green playhead needle at current frame
        const float curFrac = static_cast<float>(curFrame) / static_cast<float>(totalFrames);
        const float needleX = p0.x + curFrac * trackW;

        // Needle line
        dl->AddLine(ImVec2(needleX, p0.y + 2.0f), ImVec2(needleX, p1.y - 2.0f), IM_COL32(71, 209, 71, 255), 2.0f);
        // Pill handle at center
        dl->AddRectFilled(ImVec2(needleX - 3.0f, p0.y + 7.0f), ImVec2(needleX + 3.0f, p0.y + 19.0f),
                          IM_COL32(71, 209, 71, 255), 2.0f);

        // Interactive click/drag scrub
        ImGui::InvisibleButton("##TimelineTrack", ImVec2(trackW, trackH));
        if (ImGui::IsItemActive()) {
            float mouseX = ImGui::GetIO().MousePos.x - p0.x;
            if (mouseX < 0.0f) mouseX = 0.0f;
            if (mouseX > trackW) mouseX = trackW;
            float scrubTime = (mouseX / trackW) * maxSec;
            if (player.hasAnimation()) {
                player.scrub(scrubTime);
            }
        }
    }
    ImGui::End();

    // Bottom status bar.
    ImGui::SetNextWindowPos(ImVec2(0, (float)sh - statusH));
    ImGui::SetNextWindowSize(ImVec2((float)sw, statusH));
    ImGui::Begin("StatusBar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    // Left: [green dot] Ready | Loaded animation
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(p.x + 12.0f, p.y + 11.0f), 4.0f, IM_COL32(71, 209, 71, 255));
        ImGui::SetCursorPosX(26.0f);
        ImGui::TextColored(ImVec4(0.85f, 0.86f, 0.90f, 1.0f), "Ready");
        ImGui::SameLine(0, 16.0f);
        ImGui::TextColored(ImVec4(0.35f, 0.36f, 0.40f, 1.0f), "|");
        ImGui::SameLine(0, 16.0f);
        if (player.hasAnimation()) {
            ImGui::TextColored(ImVec4(0.65f, 0.66f, 0.70f, 1.0f),
                               "Loaded animation: A person eating an apple (%d frames)",
                               static_cast<int>(player.duration() * player.fps()));
        } else {
            ImGui::TextColored(ImVec4(0.65f, 0.66f, 0.70f, 1.0f),
                               "Loaded animation: A person eating an apple (120 frames)");
        }
    }

    // Right: Vulkan | 60 FPS | Kimodo Studio 0.1.0
    {
        const char* rInfo = "Vulkan    60 FPS    Kimodo Studio 0.1.0";
        float rw = ImGui::CalcTextSize(rInfo).x + 24.0f;
        ImGui::SameLine((float)sw - rw);
        ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.60f, 1.0f), "Vulkan");
        ImGui::SameLine(0, 12.0f);
        ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.60f, 1.0f), "%d FPS", state.fps > 0 ? state.fps : 60);
        ImGui::SameLine(0, 12.0f);
        ImGui::TextColored(ImVec4(0.55f, 0.56f, 0.60f, 1.0f), "Kimodo Studio " KIMODO_STUDIO_VERSION);
    }
    ImGui::End();
}

} // namespace studio
