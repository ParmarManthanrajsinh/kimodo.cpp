#include "app/Application.h"
#include "animation/Skeleton.h"
#include "app/SettingsManager.h"
#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

#include <filesystem>

namespace studio {
namespace {

void loadStudioFonts() {
    ImGuiIO& io = ImGui::GetIO();

    // 1. Text font: Roboto-Regular.ttf
    std::filesystem::path robotoPath = AppPaths::resolveFont("Roboto-Regular.ttf");
    std::error_code ec;
    if (std::filesystem::is_regular_file(robotoPath, ec) && !ec) {
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
        io.Fonts->AddFontFromFileTTF(robotoPath.string().c_str(), 14.0f, &textCfg, textRanges);
        Logger::instance().info("Text font (Roboto): " + robotoPath.string());
    } else {
        io.Fonts->AddFontDefault();
        Logger::instance().info("Text font: default fallback");
    }

    // 2. Symbol font: Font Awesome Solid (fa-solid-900.ttf) merged for icon codepoints
    std::filesystem::path faPath = AppPaths::resolveFont("fa-solid-900.ttf");
    if (std::filesystem::is_regular_file(faPath, ec) && !ec) {
        ImFontConfig cfg{};
        cfg.MergeMode = true;
        cfg.PixelSnapH = true;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        static const ImWchar ranges[] = {0xf000, 0xf8ff, 0};
        io.Fonts->AddFontFromFileTTF(faPath.string().c_str(), 13.0f, &cfg, ranges);
        Logger::instance().info("FA icons: " + faPath.string());
    } else {
        Logger::instance().info("FA icons: font missing, text fallback");
    }
}

} // namespace

bool Application::init(int width, int height) {
    Logger::instance().init(Logger::defaultLogFile());
    Logger::instance().info(std::string("Kimodo Studio ") + KIMODO_STUDIO_VERSION +
                            " (" + KIMODO_STUDIO_GIT_HASH + ") built " +
                            KIMODO_STUDIO_BUILD_DATE);

    AppPaths::ensureDirectories();
    SettingsManager::instance().load();
    const auto& settings = SettingsManager::instance().settings();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(width, height, "Kimodo Studio " KIMODO_STUDIO_VERSION);
    if (!IsWindowReady()) {
        Logger::instance().error("Raylib window init failed");
        return false;
    }

    // App window icon
    std::filesystem::path iconPath = AppPaths::resolveAsset("nvidia-logo.png");
    std::error_code ec;
    if (std::filesystem::is_regular_file(iconPath, ec) && !ec) {
        Image appIcon = LoadImage(iconPath.string().c_str());
        if (appIcon.data) {
            SetWindowIcon(appIcon);
            UnloadImage(appIcon);
        }
    }

    SetTargetFPS(settings.targetFps > 0 ? settings.targetFps : 60);

    rlImGuiSetLoadFontsCallback(loadStudioFonts);
    rlImGuiSetup(true);
    Theme::apply();

    // Initialize systems
    state_.gpuName = "GPU Tensor Accelerated";
    std::filesystem::path modelsJson = AppPaths::resolveConfig("models.json");
    std::filesystem::path bundlePath = AppPaths::resolveTextBundle("llm2vec-text-bundle");
    Logger::instance().info("Text bundle path: " + bundlePath.string());

    models_.init(modelsJson.string(), AppPaths::defaultModelsDir().string(), bundlePath.string());

    // Active model
    ModelEntry activeModel;
    if (models_.findCopy(models_.activeId(), activeModel) && activeModel.installed) {
        state_.motionPath = activeModel.localPath;
        Logger::instance().info("Active model: " + activeModel.name + " (" + activeModel.localPath + ")");
    }
    engine_.setPaths(state_.motionPath, bundlePath.string());

    library_.init(AppPaths::defaultAnimationsDir());
    characters_.init();

    // Viewport defaults from settings
    viewport_.reset();
    viewport_.setGrid(settings.showGrid);
    viewport_.setAxes(settings.showAxes);
    viewport_.setFloor(settings.showFloor);
    viewport_.setSkeleton(settings.showSkeleton);
    viewport_.setCharacter(settings.showCharacter);
    viewport_.setWireframe(settings.showWireframe);
    viewport_.setBoneNames(settings.showBoneNames);

    if (characters_.activeAsset()) {
        viewport_.setCharacterAsset(characters_.activeAsset());
    }

    // Initialize standby T-pose or load initial library animation
    if (!library_.entries().empty()) {
        Animation anim;
        if (library_.loadAnimation(library_.entries().front(), anim)) {
            player_.load(anim);
            player_.play();
        }
    } else {
        std::vector<float> ident(kSomaJoints * 4, 0.0f);
        for (int i = 0; i < kSomaJoints; ++i) ident[i * 4 + 3] = 1.0f;
        float root[3] = {0.0f, 0.95f, 0.0f};
        std::vector<Vector3> restPos;
        Skeleton::forwardKinematics(ident.data(), root, restPos);
        std::vector<int> parents(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
        std::vector<std::string> names(Soma30Spec::names.begin(), Soma30Spec::names.end());
        viewport_.setPose(std::move(restPos), std::move(parents), std::move(names));
    }

    Logger::instance().info("Application initialized successfully");
    running_ = true;
    return true;
}

void Application::pollEngine() {
    // Keep engine paths synchronized with active installed model
    ModelEntry activeModel;
    if (models_.findCopy(models_.activeId(), activeModel) && activeModel.installed) {
        if (state_.motionPath != activeModel.localPath) {
            state_.motionPath = activeModel.localPath;
            engine_.setPaths(state_.motionPath, AppPaths::resolveTextBundle("llm2vec-text-bundle").string());
        }
    } else if (!state_.motionPath.empty()) {
        state_.motionPath.clear();
        engine_.setPaths("", AppPaths::resolveTextBundle("llm2vec-text-bundle").string());
    }

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
            if (library_.saveAnimation(engine_.lastPrompt(), "soma-rp-v1.1", anim, saved)) {
                toasts_.push("Animation saved to library", ToastKind::Success);
            } else {
                toasts_.push("Animation generated, library save failed", ToastKind::Warning);
            }
        }
    }
    if (s == EngineStatus::Error && s != lastEngineStatus_) {
        toasts_.push(engine_.message(), ToastKind::Error);
    }
    lastEngineStatus_ = s;
}

void Application::updateAnimationAndSkinning() {
    float dt = GetFrameTime();
    player_.update(dt * state_.playbackSpeed);

    // Synchronize viewport display and transform options from state
    viewport_.setGrid(state_.showGrid);
    viewport_.setAxes(state_.showAxes);
    viewport_.setFloor(state_.showFloor);
    viewport_.setSkeleton(state_.showSkeleton);
    viewport_.setCharacter(state_.showCharacter);
    viewport_.setWireframe(state_.showWireframe);
    viewport_.setBoneNames(state_.showBoneNames);
    viewport_.setProjection(state_.cameraProjection);
    viewport_.setModelTransform(state_.modelPosition, state_.modelRotation, state_.modelScale);

    const Animation& curAnim = player_.animation();
    if (!curAnim.empty()) {
        // 1. SKELETON VIEW: pristine original SOMA skeleton directly from AnimationPlayer
        if (state_.showSkeleton) {
            viewport_.setPose(player_.worldPositions(), curAnim.parents, curAnim.jointNames);
        } else {
            viewport_.setPose({}, {});
        }

        // 2. CHARACTER PREVIEW: view/consumer of animation, never mutates original motion
        CharacterAsset* charAsset = characters_.activeAsset();
        if (state_.showCharacter && charAsset && charAsset->isLoaded()) {
            CharacterEntry curEntry;
            if (characters_.findEntry(characters_.activeId(), curEntry)) {
                std::vector<Matrix> skinMatrices;
                if (CharacterMapper::evaluateSkinMatrices(*charAsset, curAnim, player_.frame(),
                                                         curEntry.mapping, skinMatrices)) {
                    viewport_.setCharacterSkinMatrices(std::move(skinMatrices));
                } else {
                    viewport_.setCharacterSkinMatrices({});
                }
            }
        } else {
            viewport_.setCharacterSkinMatrices({});
        }
    } else {
        viewport_.setPose({}, {});
        viewport_.setCharacterSkinMatrices({});
    }
}

void Application::run(int maxFrames, const char* screenshotPath) {
    int frameCount = 0;
    while (!WindowShouldClose() && running_) {
        state_.fps = GetFPS();
        pollEngine();
        updateAnimationAndSkinning();

        // Check if mouse hovers ImGui window to route orbit camera correctly
        ImGuiIO& io = ImGui::GetIO();
        bool mouseOverUi = io.WantCaptureMouse;
        viewport_.update(mouseOverUi);

        BeginDrawing();
        ClearBackground(Color{18, 18, 24, 255});

        // 1. Draw 3D Viewport
        viewport_.draw3D();

        // 2. Draw 3D Coordinate Orientation Gizmo at bottom-left
        float gizmoX = state_.sideWidth + 38.0f;
        float gizmoY = static_cast<float>(GetScreenHeight()) - 130.0f;
        viewport_.drawOrientationGizmo(gizmoX, gizmoY);

        // 3. Draw ImGui UI Overlays
        rlImGuiBegin();
        ui_.draw(state_, viewport_, engine_, player_, library_, characters_, models_, toasts_);
        rlImGuiEnd();

        // Headless screenshot mode capture
        if (screenshotPath && (maxFrames > 0 && frameCount >= maxFrames)) {
            TakeScreenshot(screenshotPath);
            EndDrawing();
            break;
        }

        EndDrawing();

        frameCount++;
        if (maxFrames > 0 && frameCount >= maxFrames && !screenshotPath) {
            break;
        }
    }
}

void Application::shutdown() {
    ui_.shutdown();
    rlImGuiShutdown();
    CloseWindow();
    Logger::instance().info("Application shutdown cleanly");
}

} // namespace studio
