#include "app/Application.h"

#include "animation/Skeleton.h"
#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "utils/Logger.h"

#include <filesystem>

namespace studio {
namespace {

// Font loader for rlImGui: invoked inside rlImGuiSetup AFTER the ImGui
// context exists. Never touch ImGui::GetIO() before this runs (null
// context = crash on launch). Default font always loaded; FA Solid
// merged when the TTF is found, else text fallback.
void loadStudioFonts() {
    ImGuiIO& io = ImGui::GetIO();

    // 1. Text font: Roboto-Regular.ttf
    const char* robotoCandidates[] = {
        KIMODO_STUDIO_SOURCE_DIR "/fonts/Roboto-Regular.ttf",
        "fonts/Roboto-Regular.ttf",
        "../fonts/Roboto-Regular.ttf",
    };
    const char* foundRoboto = nullptr;
    for (const char* c : robotoCandidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(c, ec) && !ec) {
            foundRoboto = c;
            break;
        }
    }

    if (foundRoboto) {
        ImFontConfig textCfg{};
        textCfg.PixelSnapH = true;
        textCfg.OversampleH = 2;
        textCfg.OversampleV = 2;
        static const ImWchar textRanges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x2000, 0x206F, // General Punctuation
            0x25A0, 0x25FF, // Geometric Shapes
            0,
        };
        io.Fonts->AddFontFromFileTTF(foundRoboto, 14.0f, &textCfg, textRanges);
        Logger::instance().info(std::string("Text font (Roboto): ") + foundRoboto);
    } else {
        io.Fonts->AddFontDefault();
        Logger::instance().info("Text font: default fallback");
    }

    // 2. Symbol font: Font Awesome Solid (fa-solid-900.ttf) merged for icon codepoints
    const char* faCandidates[] = {
        KIMODO_STUDIO_SOURCE_DIR "/fonts/fa-solid-900.ttf",
        "fonts/fa-solid-900.ttf",
        "../fonts/fa-solid-900.ttf",
    };
    const char* foundFA = nullptr;
    for (const char* c : faCandidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(c, ec) && !ec) {
            foundFA = c;
            break;
        }
    }

    if (foundFA) {
        ImFontConfig cfg{};
        cfg.MergeMode = true;
        cfg.PixelSnapH = true;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        static const ImWchar ranges[] = {0xf000, 0xf8ff, 0};
        io.Fonts->AddFontFromFileTTF(foundFA, 13.0f, &cfg, ranges);
        Logger::instance().info(std::string("FA icons: ") + foundFA);
    } else {
        Logger::instance().info("FA icons: font missing, text fallback");
    }
}

} // namespace

bool Application::init() {
    Logger::instance().init(Logger::defaultLogFile());
    Logger::instance().info(std::string("Kimodo Studio ") + KIMODO_STUDIO_VERSION +
                            " (" + KIMODO_STUDIO_GIT_HASH + ") built " +
                            KIMODO_STUDIO_BUILD_DATE);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 800,
               "Kimodo Studio " KIMODO_STUDIO_VERSION " (" KIMODO_STUDIO_GIT_HASH ")");
    if (!IsWindowReady()) {
        Logger::instance().error("Raylib window init failed");
        return false;
    }

    {
        const char* iconCandidates[] = {
            KIMODO_STUDIO_SOURCE_DIR "/nvidia-logo.png",
            "nvidia-logo.png",
            "../nvidia-logo.png",
        };
        for (const char* ic : iconCandidates) {
            std::error_code ec;
            if (std::filesystem::is_regular_file(ic, ec) && !ec) {
                Image appIcon = LoadImage(ic);
                if (appIcon.data) {
                    SetWindowIcon(appIcon);
                    UnloadImage(appIcon);
                    Logger::instance().info(std::string("Window icon set: ") + ic);
                    break;
                }
            }
        }
    }

    SetTargetFPS(60);

    rlImGuiSetLoadFontsCallback(loadStudioFonts);
    rlImGuiSetup(true);
    Theme::apply();

    state_.gpuName = "RTX 4060";
    models_.init(std::string(KIMODO_STUDIO_SOURCE_DIR) + "/config/models.json",
                 "E:/kimodo.cpp/models", state_.textBundle);
    // Active registry model wins over baked-in defaults.
    {
        ModelEntry active;
        if (models_.findCopy(models_.activeId(), active) && active.installed) {
            state_.motionPath = active.localPath;
        }
    }
    engine_.setPaths(state_.motionPath, state_.textBundle);
    library_.init(AnimationLibrary::defaultBaseDir());
    viewport_.reset();
    {
        std::vector<float> ident(kSomaJoints * 4, 0.0f);
        for (int i = 0; i < kSomaJoints; ++i) ident[i * 4 + 3] = 1.0f;
        float root[3] = {0.0f, 0.95f, 0.0f};
        std::vector<Vector3> restPos;
        Skeleton::forwardKinematics(ident.data(), root, restPos);
        std::vector<int> parents(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
        viewport_.setPose(std::move(restPos), std::move(parents));
    }
    Logger::instance().info("Window + ImGui ready");
    running_ = true;
    return true;
}

void Application::pollEngine() {
    EngineStatus s = engine_.status();
    if (s == EngineStatus::Finished && s != lastEngineStatus_) {
        MotionResult result;
        if (engine_.lastResult(result)) {
            Animation anim;
            anim.fromMotionResult(result);
            player_.load(anim);
            Logger::instance().info("Viewport: animation loaded, " +
                                    std::to_string(anim.frames) + " frames");
            LibraryEntry saved;
            if (library_.saveAnimation(engine_.lastPrompt(), "soma-rp-v1.1", anim,
                                       saved)) {
                toasts_.push("Animation saved to library", ToastKind::Success);
                pendingThumb_ = AnimationLibrary::thumbPath(saved).string();
            } else {
                toasts_.push("Animation generated, library save failed",
                             ToastKind::Warning);
            }
        }
    }
    if (s == EngineStatus::Error && s != lastEngineStatus_) {
        toasts_.push(engine_.message(), ToastKind::Error);
    }
    lastEngineStatus_ = s;
}

void Application::captureThumbFile(const LibraryEntry& entry) {
    Animation anim;
    if (!library_.loadAnimation(entry, anim)) {
        toasts_.push("Could not open animation", ToastKind::Error);
        return;
    }
    player_.load(anim);
    pendingThumb_ = AnimationLibrary::thumbPath(entry).string();
    toasts_.push("Thumbnail captured", ToastKind::Success);
}

void Application::run(int maxFrames, const char* screenshotPath) {
    int frameCount = 0;
    while (running_ && !WindowShouldClose()) {
        frameCount++;
        state_.fps = GetFPS();
        pollEngine();
        player_.update(GetFrameTime());
        if (player_.hasAnimation()) {
            viewport_.setPose(player_.worldPositions(), player_.poseParents());
        }

        BeginDrawing();
        ClearBackground(Color{14, 14, 18, 255});
        rlImGuiBegin(); // fresh IO: WantCaptureMouse valid below
        viewport_.update(ImGui::GetIO().WantCaptureMouse);
        // Manager task completion surfaces as a toast (edge-triggered).
        {
            const bool busyNow = models_.busy();
            if (lastManagerBusy_ && !busyNow) {
                const std::string label = models_.taskLabel();
                if (!label.empty() && label != "idle") {
                    toasts_.push(label, ToastKind::Info);
                }
            }
            lastManagerBusy_ = busyNow;
        }
        // Model selection changes apply to the engine (model reloads lazily).
        // Edge-triggered on active id so Settings custom paths are not clobbered.
        {
            const std::string activeId = models_.activeId();
            if (lastActiveId_.empty()) {
                lastActiveId_ = activeId;
            }
            if (activeId != lastActiveId_) {
                lastActiveId_ = activeId;
                ModelEntry active;
                if (models_.findCopy(activeId, active) && active.installed &&
                    !engine_.busy()) {
                    state_.motionPath = active.localPath;
                    engine_.setPaths(state_.motionPath, state_.textBundle);
                    engine_.unloadModel();
                }
            }
        }
        if (!pendingThumb_.empty() && player_.hasAnimation()) {
            // Clean 3D-only frame for the library thumbnail (no UI overlay).
            rlImGuiEnd();
            viewport_.draw3D();
            EndDrawing();
            TakeScreenshot(pendingThumb_.c_str());
            pendingThumb_.clear();
            continue;
        }
        viewport_.draw3D();
        ui_.draw(state_, viewport_, engine_, player_, library_, models_, toasts_,
                 [this](const LibraryEntry& e) { captureThumbFile(e); });
        toasts_.draw();
        rlImGuiEnd();
        if (maxFrames > 0 && frameCount >= maxFrames) {
            if (screenshotPath) {
                TakeScreenshot(screenshotPath);
            }
            EndDrawing();
            break;
        }
        EndDrawing();
    }
}

void Application::shutdown() {
    engine_.shutdown();
    models_.shutdown();
    ui_.shutdown();
    rlImGuiShutdown();
    if (IsWindowReady()) {
        CloseWindow();
    }
    Logger::instance().info("Kimodo Studio shutdown clean");
    running_ = false;
}

} // namespace studio
