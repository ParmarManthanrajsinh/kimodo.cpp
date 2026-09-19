#include "app/Application.h"
#include "animation/Skeleton.h"
#include "app/SettingsManager.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "raylib.h"
#include "rlgl.h"
#include "ui/Theme.h"
#include "utils/AppPaths.h"
#include "utils/Logger.h"

#include <GLFW/glfw3.h>
#include <filesystem>

#ifndef KIMODO_STUDIO_VERSION
#define KIMODO_STUDIO_VERSION "0.1.0"
#endif

#ifndef KIMODO_STUDIO_GIT_HASH
#define KIMODO_STUDIO_GIT_HASH "dev"
#endif

#ifndef KIMODO_STUDIO_BUILD_DATE
#define KIMODO_STUDIO_BUILD_DATE "dev"
#endif

namespace studio
{
namespace
{

void load_studio_fonts()
{
    ImGuiIO& io = ImGui::GetIO();

    // 1. Text font: Roboto-Regular.ttf
    std::filesystem::path roboto_path = AppPaths::ResolveFont("Roboto-Regular.ttf");
    std::error_code ec;
    if (std::filesystem::is_regular_file(roboto_path, ec) && !ec)
    {
        ImFontConfig text_cfg{};
        text_cfg.PixelSnapH = true;
        text_cfg.OversampleH = 2;
        text_cfg.OversampleV = 2;
        static const ImWchar text_ranges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x2000, 0x206F, // General Punctuation
            0x25A0, 0x25FF, // Geometric Shapes
            0,
        };
        io.Fonts->AddFontFromFileTTF(roboto_path.string().c_str(), 14.0f, &text_cfg, text_ranges);
        Logger::GetInstance().Info("Text font (Roboto): " + roboto_path.string());
    }
    else
    {
        io.Fonts->AddFontDefault();
        Logger::GetInstance().Info("Text font: default fallback");
    }

    // 2. Symbol font: Font Awesome Solid (fa-solid-900.ttf) merged for icon codepoints
    std::filesystem::path fa_path = AppPaths::ResolveFont("fa-solid-900.ttf");
    if (std::filesystem::is_regular_file(fa_path, ec) && !ec)
    {
        ImFontConfig cfg{};
        cfg.MergeMode = true;
        cfg.PixelSnapH = true;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        static const ImWchar ranges[] = {0xf000, 0xf8ff, 0};
        io.Fonts->AddFontFromFileTTF(fa_path.string().c_str(), 13.0f, &cfg, ranges);
        Logger::GetInstance().Info("FA icons: " + fa_path.string());
    }
    else
    {
        Logger::GetInstance().Info("FA icons: font missing, text fallback");
    }
}

} // namespace

bool Application::Init(int width, int height)
{
    Logger::GetInstance().Init(Logger::DefaultLogFile());
    Logger::GetInstance().Info(std::string("Kimodo Studio ") + KIMODO_STUDIO_VERSION + " (" + KIMODO_STUDIO_GIT_HASH +
                               ") built " + KIMODO_STUDIO_BUILD_DATE);

    AppPaths::EnsureDirectories();
    SettingsManager::GetInstance().Load();
    const auto& settings = SettingsManager::GetInstance().GetSettings();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(width, height, "Kimodo Studio " KIMODO_STUDIO_VERSION);
    SetWindowMinSize(800, 500);
    if (!IsWindowReady())
    {
        Logger::GetInstance().Error("Raylib window init failed");
        return false;
    }

    // App window icon
    std::filesystem::path icon_path = AppPaths::ResolveAsset("nvidia-logo.png");
    std::error_code ec;
    if (std::filesystem::is_regular_file(icon_path, ec) && !ec)
    {
        Image app_icon = LoadImage(icon_path.string().c_str());
        if (app_icon.data)
        {
            SetWindowIcon(app_icon);
            UnloadImage(app_icon);
        }
    }

    glfw_window = static_cast<GLFWwindow*>(glfwGetCurrentContext());

    // Detach Raylib's GLFW WindowSizeCallback so it never runs on the main thread
    // without an OpenGL context when the window is resized
    glfwSetWindowSizeCallback(glfw_window, nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    load_studio_fonts();
    Theme::Apply();
    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);

    // Initialize systems
    state.gpu_name = "GPU Tensor Accelerated";
    std::filesystem::path models_json = AppPaths::ResolveConfig("models.json");
    std::filesystem::path bundle_path = AppPaths::ResolveTextBundle("llm2vec-text-bundle");
    Logger::GetInstance().Info("Text bundle path: " + bundle_path.string());

    models.Init(models_json.string(), AppPaths::DefaultModelsDir().string(), bundle_path.string());

    // Active model
    ModelEntry active_model;
    if (models.find_copy(models.GetActiveId(), active_model) && active_model.installed)
    {
        state.motion_path = active_model.local_path;
        Logger::GetInstance().Info("Active model: " + active_model.name + " (" + active_model.local_path + ")");
    }
    engine.SetPaths(state.motion_path, bundle_path.string());

    library.Init(AppPaths::DefaultAnimationsDir());
    characters.Init();

    // Viewport defaults from settings
    viewport.Reset();
    viewport.SetGrid(settings.show_grid);
    viewport.SetAxes(settings.show_axes);
    viewport.SetFloor(settings.show_floor);
    viewport.SetSkeleton(settings.show_skeleton);
    viewport.SetCharacter(settings.show_character);
    viewport.SetWireframe(settings.show_wireframe);
    viewport.SetBoneNames(settings.show_bone_names);

    if (characters.GetActiveAsset())
    {
        viewport.SetCharacterAsset(characters.GetActiveAsset());
    }

    // Initialize standby T-pose or load initial library animation
    if (!library.GetEntries().empty())
    {
        Animation anim;
        if (library.LoadAnimation(library.GetEntries().front(), anim))
        {
            player.Load(anim);
            player.Play();
        }
    }
    else
    {
        std::vector<float> ident(kSomaJoints * 4, 0.0f);
        for (int i = 0; i < kSomaJoints; ++i)
            ident[i * 4 + 3] = 1.0f;
        float root[3] = {0.0f, 0.95f, 0.0f};
        std::vector<Vector3> rest_pos;
        Skeleton::ForwardKinematics(ident.data(), root, rest_pos);
        std::vector<int> parents(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
        std::vector<std::string> names(Soma30Spec::names.begin(), Soma30Spec::names.end());
        viewport.SetPose(std::move(rest_pos), std::move(parents), std::move(names));
    }

    Logger::GetInstance().Info("Application initialized successfully");
    running = true;
    return true;
}

void Application::PollEngine()
{
    // Keep engine paths synchronized with active installed model
    ModelEntry active_model;
    if (models.find_copy(models.GetActiveId(), active_model) && active_model.installed)
    {
        if (state.motion_path != active_model.local_path)
        {
            state.motion_path = active_model.local_path;
            engine.SetPaths(state.motion_path, AppPaths::ResolveTextBundle("llm2vec-text-bundle").string());
        }
    }
    else if (!state.motion_path.empty())
    {
        state.motion_path.clear();
        engine.SetPaths("", AppPaths::ResolveTextBundle("llm2vec-text-bundle").string());
    }

    EngineStatus s = engine.GetStatus();
    if (s == EngineStatus::Finished && s != last_engine_status)
    {
        MotionResult result;
        if (engine.LastResult(result))
        {
            Animation anim;
            anim.FromMotionResult(result);
            player.Load(anim);
            Logger::GetInstance().Info("Viewport: animation loaded, " + std::to_string(anim.frames) + " frames");
            LibraryEntry saved;
            if (library.SaveAnimation(engine.GetLastPrompt(), "soma-rp-v1.1", anim, saved))
            {
                toasts.Push("Animation saved to library", ToastKind::Success);
            }
            else
            {
                toasts.Push("Animation generated, library save failed", ToastKind::Warning);
            }
        }
    }
    if (s == EngineStatus::Error && s != last_engine_status)
    {
        toasts.Push(engine.GetMessage(), ToastKind::Error);
    }
    last_engine_status = s;
}

void Application::UpdateAnimationAndSkinning(float dt)
{
    player.Update(dt * state.playback_speed);

    // Synchronize viewport display and transform options from state
    viewport.SetGrid(state.show_grid);
    viewport.SetAxes(state.show_axes);
    viewport.SetFloor(state.show_floor);
    viewport.SetSkeleton(state.show_skeleton);
    viewport.SetCharacter(state.show_character);
    viewport.SetWireframe(state.show_wireframe);
    viewport.SetBoneNames(state.show_bone_names);
    viewport.SetProjection(state.camera_projection);
    viewport.SetModelTransform(state.model_position, state.model_rotation, state.model_scale);

    const Animation& cur_anim = player.GetAnimation();
    if (!cur_anim.empty())
    {
        // 1. SKELETON VIEW: pristine original SOMA skeleton directly from AnimationPlayer
        if (state.show_skeleton)
        {
            viewport.SetPose(player.GetWorldPositions(), cur_anim.parents, cur_anim.joint_names);
        }
        else
        {
            viewport.SetPose({}, {});
        }

        // 2. CHARACTER PREVIEW: view/consumer of animation, never mutates original motion
        CharacterAsset* char_asset = characters.GetActiveAsset();
        if (state.show_character && char_asset && char_asset->IsLoaded())
        {
            CharacterEntry cur_entry;
            if (characters.FindEntry(characters.GetActiveId(), cur_entry))
            {
                std::vector<Matrix> skin_matrices;
                if (CharacterMapper::EvaluateSkinMatrices(*char_asset, cur_anim, player.Frame(), cur_entry.mapping,
                                                          skin_matrices))
                {
                    viewport.SetCharacterSkinMatrices(std::move(skin_matrices));
                }
                else
                {
                    viewport.SetCharacterSkinMatrices({});
                }
            }
        }
        else
        {
            viewport.SetCharacterSkinMatrices({});
        }
    }
    else
    {
        viewport.SetPose({}, {});
        viewport.SetCharacterSkinMatrices({});
    }
}

void Application::RenderLoop()
{
    glfwMakeContextCurrent(glfw_window);
    glfwSwapInterval(1);

    ImGui_ImplOpenGL3_Init("#version 330");

    while (running)
    {
        RenderFrame();
    }

    ImGui_ImplOpenGL3_Shutdown();
    glfwMakeContextCurrent(nullptr);
    render_finished = true;
}

void Application::RenderFrame()
{
    if (!running || !glfw_window)
    {
        return;
    }

    int win_w = 0, win_h = 0;
    glfwGetFramebufferSize(glfw_window, &win_w, &win_h);
    if (win_w <= 0 || win_h <= 0)
    {
        return;
    }

    double cur_time = glfwGetTime();
    float dt = (last_frame_time > 0.0) ? static_cast<float>(cur_time - last_frame_time) : (1.0f / 60.0f);
    last_frame_time = cur_time;
    if (dt > 0.1f)
    {
        dt = 0.1f;
    }
    if (dt < 0.0f)
    {
        dt = 0.0f;
    }

    state.fps = static_cast<int>(ImGui::GetIO().Framerate);
    PollEngine();
    UpdateAnimationAndSkinning(dt);

    // 1. Synchronize Viewport dimensions from UI layout (Raylib-ImGui-Hybrid pattern)
    if (state.desired_viewport_width > 0 && state.desired_viewport_height > 0 &&
        (state.desired_viewport_width != viewport.GetWidth() ||
         state.desired_viewport_height != viewport.GetHeight()))
    {
        state.viewport_width = state.desired_viewport_width;
        state.viewport_height = state.desired_viewport_height;
        viewport.EnsureSize(state.viewport_width, state.viewport_height);
    }
    else if (!viewport.IsReady())
    {
        viewport.EnsureSize(state.viewport_width, state.viewport_height);
    }

    // 2. Offscreen Render Pass: render 3D scene directly to RenderTexture2D
    if (viewport.IsReady())
    {
        viewport.BeginRender();
        viewport.Draw3D();
        viewport.DrawOrientationGizmo(38.0f, static_cast<float>(viewport.GetHeight()) - 40.0f);
        viewport.EndRender();
    }

    // 3. Main Backbuffer & ImGui Display Pass (camera navigation handled inside Viewport window)
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ui.Draw(state, viewport, engine, player, library, characters, models, toasts);

    ImGui::Render();

    // 5. Backbuffer Clear & Render ImGui DrawData (matching Raylib-ImGui-Hybrid)
    glViewport(0, 0, win_w, win_h);
    glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(glfw_window);
}

void Application::Run(int max_frames, const char* screenshot_path)
{
    if (screenshot_path && max_frames > 0)
    {
        ImGui_ImplOpenGL3_Init("#version 330");
        int frame_count = 0;
        while (!WindowShouldClose() && running && frame_count < max_frames)
        {
            RenderFrame();
            frame_count++;
        }
        TakeScreenshot(screenshot_path);
        ImGui_ImplOpenGL3_Shutdown();
        return;
    }

    // Decoupled Multi-threaded Desktop Pipeline from Raylib-ImGui-Hybrid:
    // 1. Release OpenGL context from main thread
    glfwMakeContextCurrent(nullptr);
    render_finished = false;

    // 2. Spawn dedicated RenderThread
    render_thread = std::thread(&Application::RenderLoop, this);

    // 3. Main thread drives OS event loop continuously
    while (running)
    {
        glfwWaitEvents();
        if (glfwWindowShouldClose(glfw_window))
        {
            running = false;
        }
    }

    // 4. Await render thread completion
    while (!render_finished)
    {
        glfwWaitEventsTimeout(0.005);
    }

    if (render_thread.joinable())
    {
        render_thread.join();
    }

    // 5. Restore context to main thread for shutdown
    glfwMakeContextCurrent(glfw_window);
}

void Application::Shutdown()
{
    running = false;
    if (render_thread.joinable())
    {
        render_thread.join();
    }

    if (glfw_window)
    {
        glfwMakeContextCurrent(glfw_window);
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    viewport.Shutdown();
    ui.Shutdown();
    CloseWindow();
    Logger::GetInstance().Info("Application shutdown cleanly");
}

} // namespace studio
