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
    std::filesystem::path robotoPath = FAppPaths::resolveFont("Roboto-Regular.ttf");
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
        FLogger::GetInstance().info("Text font (Roboto): " + robotoPath.string());
    } else {
        io.Fonts->AddFontDefault();
        FLogger::GetInstance().info("Text font: default fallback");
    }

    // 2. Symbol font: Font Awesome Solid (fa-solid-900.ttf) merged for icon codepoints
    std::filesystem::path faPath = FAppPaths::resolveFont("fa-solid-900.ttf");
    if (std::filesystem::is_regular_file(faPath, ec) && !ec) {
        ImFontConfig cfg{};
        cfg.MergeMode = true;
        cfg.PixelSnapH = true;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        static const ImWchar ranges[] = {0xf000, 0xf8ff, 0};
        io.Fonts->AddFontFromFileTTF(faPath.string().c_str(), 13.0f, &cfg, ranges);
        FLogger::GetInstance().info("FA icons: " + faPath.string());
    } else {
        FLogger::GetInstance().info("FA icons: font missing, text fallback");
    }
}

} // namespace

bool FApplication::Init(int width, int height) {
    FLogger::GetInstance().Init(FLogger::DefaultLogFile());
    FLogger::GetInstance().info(std::string("Kimodo Studio ") + KIMODO_STUDIO_VERSION +
                            " (" + KIMODO_STUDIO_GIT_HASH + ") built " +
                            KIMODO_STUDIO_BUILD_DATE);

    FAppPaths::ensureDirectories();
    FSettingsManager::GetInstance().load();
    const auto& settings = FSettingsManager::GetInstance().GetSettings();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(width, height, "Kimodo Studio " KIMODO_STUDIO_VERSION);
    if (!IsWindowReady()) {
        FLogger::GetInstance().error("Raylib window init failed");
        return false;
    }

    // App window icon
    std::filesystem::path iconPath = FAppPaths::resolveAsset("nvidia-logo.png");
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
    FTheme::apply();

    // Initialize systems
    state.gpuName = "GPU Tensor Accelerated";
    std::filesystem::path modelsJson = FAppPaths::resolveConfig("models.json");
    std::filesystem::path bundlePath = FAppPaths::resolveTextBundle("llm2vec-text-bundle");
    FLogger::GetInstance().info("Text bundle path: " + bundlePath.string());

    models.Init(modelsJson.string(), FAppPaths::defaultModelsDir().string(), bundlePath.string());

    // Active model
    FModelEntry activeModel;
    if (models.findCopy(models.GetActiveId(), activeModel) && activeModel.installed) {
        state.motionPath = activeModel.localPath;
        FLogger::GetInstance().info("Active model: " + activeModel.name + " (" + activeModel.localPath + ")");
    }
    engine.SetPaths(state.motionPath, bundlePath.string());

    library.Init(FAppPaths::defaultAnimationsDir());
    characters.Init();

    // Viewport defaults from settings
    viewport.Reset();
    viewport.SetGrid(settings.showGrid);
    viewport.SetAxes(settings.showAxes);
    viewport.SetFloor(settings.showFloor);
    viewport.SetSkeleton(settings.showSkeleton);
    viewport.SetCharacter(settings.showCharacter);
    viewport.SetWireframe(settings.showWireframe);
    viewport.SetBoneNames(settings.showBoneNames);

    if (characters.GetActiveAsset()) {
        viewport.SetCharacterAsset(characters.GetActiveAsset());
    }

    // Initialize standby T-pose or load initial library animation
    if (!library.GetEntries().empty()) {
        FAnimation anim;
        if (library.loadAnimation(library.GetEntries().front(), anim)) {
            player.load(anim);
            player.play();
        }
    } else {
        std::vector<float> ident(kSomaJoints * 4, 0.0f);
        for (int i = 0; i < kSomaJoints; ++i) ident[i * 4 + 3] = 1.0f;
        float root[3] = {0.0f, 0.95f, 0.0f};
        std::vector<Vector3> restPos;
        FSkeleton::ForwardKinematics(ident.data(), root, restPos);
        std::vector<int> parents(FSoma30Spec::parents.begin(), FSoma30Spec::parents.end());
        std::vector<std::string> names(FSoma30Spec::names.begin(), FSoma30Spec::names.end());
        viewport.SetPose(std::move(restPos), std::move(parents), std::move(names));
    }

    FLogger::GetInstance().info("Application initialized successfully");
    bRunning = true;
    return true;
}

void FApplication::PollEngine() {
    // Keep engine paths synchronized with active installed model
    FModelEntry activeModel;
    if (models.findCopy(models.GetActiveId(), activeModel) && activeModel.installed) {
        if (state.motionPath != activeModel.localPath) {
            state.motionPath = activeModel.localPath;
            engine.SetPaths(state.motionPath, FAppPaths::resolveTextBundle("llm2vec-text-bundle").string());
        }
    } else if (!state.motionPath.empty()) {
        state.motionPath.clear();
        engine.SetPaths("", FAppPaths::resolveTextBundle("llm2vec-text-bundle").string());
    }

    EEngineStatus s = engine.GetStatus();
    if (s == EEngineStatus::Finished && s != lastEngineStatus) {
        FMotionResult result;
        if (engine.lastResult(result)) {
            FAnimation anim;
            anim.fromMotionResult(result);
            player.load(anim);
            FLogger::GetInstance().info("Viewport: animation loaded, " +
                                    std::to_string(anim.frames) + " frames");
            FLibraryEntry saved;
            if (library.saveAnimation(engine.GetLastPrompt(), "soma-rp-v1.1", anim, saved)) {
                toasts.Push("Animation saved to library", EToastKind::Success);
            } else {
                toasts.Push("Animation generated, library save failed", EToastKind::Warning);
            }
        }
    }
    if (s == EEngineStatus::Error && s != lastEngineStatus) {
        toasts.Push(engine.GetMessage(), EToastKind::Error);
    }
    lastEngineStatus = s;
}

void FApplication::UpdateAnimationAndSkinning() {
    float dt = GetFrameTime();
    player.Update(dt * state.playbackSpeed);

    // Synchronize viewport display and transform options from state
    viewport.SetGrid(state.showGrid);
    viewport.SetAxes(state.showAxes);
    viewport.SetFloor(state.showFloor);
    viewport.SetSkeleton(state.showSkeleton);
    viewport.SetCharacter(state.showCharacter);
    viewport.SetWireframe(state.showWireframe);
    viewport.SetBoneNames(state.showBoneNames);
    viewport.SetProjection(state.cameraProjection);
    viewport.SetModelTransform(state.modelPosition, state.modelRotation, state.modelScale);

    const FAnimation& curAnim = player.GetAnimation();
    if (!curAnim.empty()) {
        // 1. SKELETON VIEW: pristine original SOMA skeleton directly from AnimationPlayer
        if (state.showSkeleton) {
            viewport.SetPose(player.GetWorldPositions(), curAnim.parents, curAnim.jointNames);
        } else {
            viewport.SetPose({}, {});
        }

        // 2. CHARACTER PREVIEW: view/consumer of animation, never mutates original motion
        FCharacterAsset* charAsset = characters.GetActiveAsset();
        if (state.showCharacter && charAsset && charAsset->IsLoaded()) {
            FCharacterEntry curEntry;
            if (characters.FindEntry(characters.GetActiveId(), curEntry)) {
                std::vector<Matrix> skinMatrices;
                if (FCharacterMapper::evaluateSkinMatrices(*charAsset, curAnim, player.Frame(),
                                                         curEntry.mapping, skinMatrices)) {
                    viewport.SetCharacterSkinMatrices(std::move(skinMatrices));
                } else {
                    viewport.SetCharacterSkinMatrices({});
                }
            }
        } else {
            viewport.SetCharacterSkinMatrices({});
        }
    } else {
        viewport.SetPose({}, {});
        viewport.SetCharacterSkinMatrices({});
    }
}

void FApplication::Run(int maxFrames, const char* screenshotPath) {
    int frameCount = 0;
    while (!WindowShouldClose() && bRunning) {
        state.fps = GetFPS();
        PollEngine();
        UpdateAnimationAndSkinning();

        // Check if mouse hovers ImGui window to route orbit camera correctly
        ImGuiIO& io = ImGui::GetIO();
        bool mouseOverUi = io.WantCaptureMouse;
        viewport.Update(mouseOverUi);

        BeginDrawing();
        ClearBackground(Color{18, 18, 24, 255});

        // 1. Draw 3D Viewport
        viewport.Draw3D();

        // 2. Draw 3D Coordinate Orientation Gizmo at bottom-left
        float gizmoX = state.sideWidth + 38.0f;
        float gizmoY = static_cast<float>(GetScreenHeight()) - 130.0f;
        viewport.DrawOrientationGizmo(gizmoX, gizmoY);

        // 3. Draw ImGui UI Overlays
        rlImGuiBegin();
        ui.Draw(state, viewport, engine, player, library, characters, models, toasts);
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

void FApplication::Shutdown() {
    ui.Shutdown();
    rlImGuiShutdown();
    CloseWindow();
    FLogger::GetInstance().info("Application shutdown cleanly");
}

} // namespace studio
