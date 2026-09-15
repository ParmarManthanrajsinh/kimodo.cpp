# Kimodo Studio — Full Development Plan

> A native, production-ready C++ desktop application built around `kimodo.cpp`,
> using Raylib + Dear ImGui, with model management, Hugging Face integration,
> animation preview, retargeting, and game-engine-friendly export.

---

# 1. Project Vision

Build a polished native desktop application around `kimodo.cpp`.

The application should allow a user to:

1. Install the application.
2. Detect their hardware.
3. Connect/authenticate with Hugging Face when required.
4. Browse supported Kimodo models.
5. Download/install models through a GUI.
6. Verify downloaded model files.
7. Select an installed model.
8. Enter a natural-language motion prompt.
9. Generate animation locally using Kimodo.
10. Preview the generated animation in a 3D viewport.
11. Play/pause/scrub/loop the animation.
12. Inspect the skeleton.
13. Retarget the animation to common game-engine skeletons.
14. Export the animation in game-engine-friendly formats.
15. Easily move the animation into Unreal Engine, Unity, Blender, or other
    3D software.

The application should feel like a real product, not a developer demo.

---

# 2. Important Design Principle

DO NOT turn Kimodo's inference code into UI code.

Keep Kimodo as an independent inference backend.

Architecture:

    ┌──────────────────────────────────────────┐
    │              Kimodo Studio               │
    │                                          │
    │  UI / Application Layer                  │
    │  C++ + Raylib + Dear ImGui               │
    │                                          │
    ├──────────────────────────────────────────┤
    │                                          │
    │  Application Services                    │
    │  - Model Manager                         │
    │  - HF Authentication                     │
    │  - Downloader                            │
    │  - Animation Manager                     │
    │  - Retargeting                           │
    │  - Export                                │
    │  - Settings                              │
    │                                          │
    ├──────────────────────────────────────────┤
    │                                          │
    │  Kimodo Adapter                          │
    │  - C API                                 │
    │  - Generation                            │
    │  - Model loading                         │
    │  - Embedding handling                    │
    │                                          │
    ├──────────────────────────────────────────┤
    │                                          │
    │  kimodo.cpp                              │
    │  C++ / GGML / Vulkan                     │
    │                                          │
    └──────────────────────────────────────────┘

The UI must not directly manipulate GGML internals.

---

# 3. Technology Stack

## Core

- C++23
- CMake
- Ninja
- Git

## UI

- Raylib
- Dear ImGui

## Rendering

- Raylib 3D renderer
- GPU accelerated rendering
- Vulkan-capable Kimodo backend

## Inference

- kimodo.cpp
- GGML
- Vulkan

## Networking

Prefer a lightweight C++ HTTP implementation.

Possible choices:

- libcurl
- cpp-httplib
- another mature MIT/BSD/Apache-compatible library

Do not introduce a huge framework unless necessary.

## JSON

Use a lightweight JSON library such as:

- nlohmann/json

## Compression / Archives

Use a mature library if required.

## Authentication

Hugging Face token authentication.

Never store the raw token in plain text.

Use the platform's secure credential storage where possible.

## Logging

Implement application logging with levels:

- TRACE
- DEBUG
- INFO
- WARNING
- ERROR

Logs should be written to a user application-data directory.

---

# 4. Repository Structure

Create the project approximately like this:

    kimodo-studio/
    │
    ├── CMakeLists.txt
    ├── CMakePresets.json
    ├── README.md
    ├── LICENSE
    ├── PLAN.md
    │
    ├── assets/
    │   ├── fonts/
    │   ├── icons/
    │   └── branding/
    │
    ├── config/
    │   └── models.json
    │
    ├── src/
    │   │
    │   ├── main.cpp
    │   │
    │   ├── app/
    │   │   ├── Application.cpp
    │   │   ├── Application.h
    │   │   ├── AppState.cpp
    │   │   └── AppState.h
    │   │
    │   ├── ui/
    │   │   ├── UIManager.cpp
    │   │   ├── UIManager.h
    │   │   ├── Theme.cpp
    │   │   ├── Theme.h
    │   │   │
    │   │   ├── screens/
    │   │   │   ├── HomeScreen.cpp
    │   │   │   ├── GenerateScreen.cpp
    │   │   │   ├── ModelsScreen.cpp
    │   │   │   ├── AnimationScreen.cpp
    │   │   │   ├── SettingsScreen.cpp
    │   │   │   └── AboutScreen.cpp
    │   │   │
    │   │   └── widgets/
    │   │       ├── Button.cpp
    │   │       ├── ModelCard.cpp
    │   │       ├── ProgressBar.cpp
    │   │       ├── Timeline.cpp
    │   │       └── Toast.cpp
    │   │
    │   ├── rendering/
    │   │   ├── Viewport.cpp
    │   │   ├── Viewport.h
    │   │   ├── CameraController.cpp
    │   │   ├── SkeletonRenderer.cpp
    │   │   └── GridRenderer.cpp
    │   │
    │   ├── kimodo/
    │   │   ├── KimodoEngine.cpp
    │   │   ├── KimodoEngine.h
    │   │   ├── KimodoAdapter.cpp
    │   │   └── KimodoAdapter.h
    │   │
    │   ├── models/
    │   │   ├── ModelManager.cpp
    │   │   ├── ModelManager.h
    │   │   ├── ModelInfo.cpp
    │   │   ├── ModelInfo.h
    │   │   ├── ModelInstaller.cpp
    │   │   └── ModelVerifier.cpp
    │   │
    │   ├── huggingface/
    │   │   ├── HuggingFaceClient.cpp
    │   │   ├── HuggingFaceClient.h
    │   │   ├── HFAuthenticator.cpp
    │   │   └── HFAuthenticator.h
    │   │
    │   ├── download/
    │   │   ├── Downloader.cpp
    │   │   ├── Downloader.h
    │   │   └── DownloadTask.cpp
    │   │
    │   ├── animation/
    │   │   ├── Animation.cpp
    │   │   ├── Animation.h
    │   │   ├── Skeleton.cpp
    │   │   ├── Skeleton.h
    │   │   ├── AnimationPlayer.cpp
    │   │   └── AnimationPlayer.h
    │   │
    │   ├── retarget/
    │   │   ├── Retargeter.cpp
    │   │   ├── Retargeter.h
    │   │   ├── SkeletonProfile.cpp
    │   │   └── SkeletonProfile.h
    │   │
    │   ├── export/
    │   │   ├── AnimationExporter.cpp
    │   │   ├── AnimationExporter.h
    │   │   ├── GLTFExporter.cpp
    │   │   ├── GLBExporter.cpp
    │   │   └── FBXExporter.cpp
    │   │
    │   ├── hardware/
    │   │   ├── HardwareDetector.cpp
    │   │   └── HardwareDetector.h
    │   │
    │   ├── settings/
    │   │   ├── Settings.cpp
    │   │   └── Settings.h
    │   │
    │   └── utils/
    │       ├── FileSystem.cpp
    │       ├── Logger.cpp
    │       ├── Hash.cpp
    │       └── Process.cpp
    │
    ├── third_party/
    │   ├── raylib/
    │   ├── imgui/
    │   └── ...
    │
    ├── tests/
    │   ├── test_animation.cpp
    │   ├── test_retarget.cpp
    │   ├── test_models.cpp
    │   └── test_export.cpp
    │
    └── packaging/
        ├── windows/
        └── linux/

---

# 5. UI Philosophy

The application should NOT look like a default Dear ImGui application.

Avoid:

- default ImGui theme
- excessive windows
- developer-tool appearance
- too many buttons
- cluttered panels
- unnecessary information
- ugly debug controls

The application should feel similar to a modern creative application.

Design inspiration:

- Blender
- Unreal Engine
- Substance 3D
- modern AI applications
- professional animation tools

But do NOT copy their UI directly.

---

# 6. Main Application Layout

Use a persistent application shell:

    ┌────────────────────────────────────────────────────────────┐
    │ Kimodo Studio                         GPU ●    Settings ⚙  │
    ├────────────┬───────────────────────────────────────────────┤
    │            │                                               │
    │  Home      │                                               │
    │  Generate  │             Main Content                      │
    │  Models    │                                               │
    │  Library   │                                               │
    │            │                                               │
    │            │                                               │
    │  Settings  │                                               │
    │  About     │                                               │
    │            │                                               │
    └────────────┴───────────────────────────────────────────────┘

Sidebar:

- Home
- Generate
- Animations
- Models
- Settings

Bottom status bar:

- GPU
- VRAM
- Current model
- generation status

---

# 7. Home Screen

Home screen should be simple.

Display:

    Kimodo Studio

    Generate motion from text.

    [ Generate Animation ]

    Recent Animations

    [ animation card ]
    [ animation card ]
    [ animation card ]

    Installed Models

    [ model card ]

If no model is installed:

    No models installed.

    Install a model to start generating animations.

    [ Browse Models ]

---

# 8. Generate Screen

This is the primary screen.

Layout:

    ┌────────────────────────────────────────────────────────────┐
    │ Generate Animation                                         │
    ├──────────────────┬─────────────────────────────────────────┤
    │                  │                                         │
    │ Prompt           │                                         │
    │                  │                                         │
    │ ┌──────────────┐ │              3D Viewport                │
    │ │ A person     │ │                                         │
    │ │ walks...     │ │                 Skeleton                │
    │ └──────────────┘ │                                         │
    │                  │                                         │
    │ Model            │                                         │
    │ [ SOMA ▼ ]       │                                         │
    │                  │                                         │
    │ Duration         │                                         │
    │ [ 5 sec ]        │                                         │
    │                  │                                         │
    │ Steps            │                                         │
    │ [ 50 ]           │                                         │
    │                  │                                         │
    │ [ Generate ]     │                                         │
    │                  │                                         │
    └──────────────────┴─────────────────────────────────────────┘

Generation must run asynchronously.

Never freeze the UI.

---

# 9. Animation Viewport

The viewport is the most important visual component.

Use Raylib 3D.

Features:

- perspective camera
- orbit
- pan
- zoom
- reset camera
- grid
- skeleton rendering
- joint visualization
- bone visualization
- ground plane
- optional axes
- animation playback

Controls:

    Left drag      → orbit
    Middle drag    → pan
    Wheel          → zoom
    F              → frame selected animation
    R              → reset camera

---

# 10. Animation Timeline

Bottom panel:

    ┌────────────────────────────────────────────────────────────┐
    │ ▶  ◀  0:00 ━━━━━━━━━━━━━━━●━━━━━━━━━━━━ 0:04              │
    │      0        30       60       90       120               │
    └────────────────────────────────────────────────────────────┘

Features:

- play
- pause
- restart
- loop
- frame stepping
- FPS
- current frame
- duration
- timeline scrubbing

---

# 11. Animation Library

Users should be able to save generated animations.

Each animation should contain:

    {
        "id": "...",
        "prompt": "...",
        "model": "...",
        "created_at": "...",
        "fps": 30,
        "frames": 150,
        "skeleton": "soma",
        "file": "..."
    }

Display:

- thumbnail
- prompt
- model
- date
- duration

Actions:

- Open
- Rename
- Export
- Delete
- Duplicate

---

# 12. Model Manager

This is a major part of the application.

Screen:

    Models

    Installed
    ───────────────

    SOMA RP v1.1
    ✓ Installed
    4.2 GB

    [ Open ] [ Remove ]


    Available Models
    ─────────────────

    SOMA RP v1.1
    G1 RP v1
    G1 SEED v1

    [ Install ]

Model cards should show:

- model name
- skeleton
- size
- version
- license
- source
- compatibility
- installed status

---

# 13. Model Installation Wizard

Installing a model should be a proper workflow.

Step 1:

    Select Model

Step 2:

    License

    Read and accept applicable model terms.

Step 3:

    Hugging Face authentication if required.

Step 4:

    Select installation location.

Step 5:

    Download

    ███████████████░░░░ 74%

    3.1 GB / 4.2 GB

    42 MB/s

Step 6:

    Verify

    Checking SHA-256...

Step 7:

    Installed successfully.

---

# 14. Hugging Face Integration

Implement:

- login
- logout
- token validation
- repository access
- file listing
- downloads
- authentication errors

Never hardcode tokens.

Never put Hugging Face credentials into source code.

Never log tokens.

Prefer OS secure credential storage.

Possible UX:

    Hugging Face

    Status:
    ● Connected

    Account:
    username

    [ Disconnect ]

For unauthenticated users:

    Hugging Face account required.

    [ Connect ]

Do not create an unnecessary browser-based account system.

Use Hugging Face's supported authentication mechanisms.

---

# 15. Model Licensing

This is critical.

Do NOT assume every Kimodo model can legally be redistributed.

Maintain explicit metadata:

    model:
        id
        repository
        license
        redistribution_allowed
        commercial_allowed
        requires_authentication
        checksum
        size

Only expose models that are approved for this application.

Restricted models must NOT be silently packaged with the application.

If a model requires the user to download it themselves:

    "This model cannot be redistributed by Kimodo Studio.
     You can download it directly from the official source."

The application should make model licensing explicit.

---

# 16. Recommended Initial Model Strategy

Start with models whose upstream license permits the intended distribution/use.

Do NOT initially package restrictive models.

For the first production version:

    Kimodo Studio
        ↓
    Supported redistributable model list
        ↓
    Hugging Face
        ↓
    Download
        ↓
    Verify
        ↓
    Install

Keep the model registry configurable so additional models can be added later.

---

# 17. Hardware Detection

At startup detect:

- CPU
- RAM
- GPU
- VRAM
- Vulkan availability
- operating system
- architecture

Example:

    System

    GPU
    NVIDIA GeForce RTX 4060

    VRAM
    8 GB

    Vulkan
    Available

    RAM
    16 GB

Use this information to choose sensible defaults.

---

# 18. Low-VRAM Strategy

The application must work on systems with limited VRAM.

Do not assume the entire model can fit into VRAM.

Provide settings:

    Performance

    Text Encoder
    [ Automatic ]

    GPU Memory
    [ Automatic ]

    Text Layer Chunk
    [ Automatic ]

    CPU Offload
    [ Automatic ]

For an RTX 4060 8 GB:

    Recommended:
        Vulkan
        GPU motion model
        CPU/offloaded text encoder
        automatic VRAM management

The application should detect available VRAM and avoid obvious allocation failures.

---

# 19. Kimodo Adapter

Create a clean adapter around the Kimodo C API.

Example:

    class KimodoEngine
    {
    public:

        bool initialize();

        bool loadModel(
            const std::filesystem::path& model
        );

        bool generate(
            const GenerationRequest& request,
            GenerationResult& result
        );

        void unload();

        bool isReady() const;
    };

The UI should only interact with this abstraction.

The UI should never directly call GGML functions.

---

# 20. Asynchronous Generation

Generation must never block the rendering thread.

Architecture:

    UI thread
        │
        ├── request generation
        │
        ▼
    Worker thread
        │
        └── Kimodo
              │
              ▼
        Animation result
              │
              ▼
        UI thread

Provide progress states:

    Loading model
    Encoding prompt
    Sampling
    Processing animation
    Exporting
    Finished

---

# 21. Internal Animation Representation

Do NOT make GLB the only internal format.

Create an internal animation representation:

    Animation
        Skeleton
            Joint[]
                name
                parent
                rest_position
                rest_rotation

        Frames
            Frame[]
                root_translation
                local_rotation[]

        fps
        duration

This becomes the central format used by:

- viewport
- timeline
- retargeting
- exporters
- animation library

---

# 22. Skeleton System

Represent skeletons generically.

Example:

    Skeleton
        joints:
            pelvis
            left_hip
            right_hip
            spine
            ...
            
Each joint:

    name
    parent
    rest transform
    local transform
    metadata

Never hardcode the application around only one skeleton.

---

# 23. Retargeting System

This is important for game-engine usage.

Kimodo generates animation for its own skeleton.

Game engines commonly use different skeletons.

Therefore:

    Kimodo Skeleton
          │
          ▼
      Retargeter
          │
          ▼
    Target Skeleton

Support skeleton profiles.

Example:

    Source:
        Kimodo SOMA

    Target:
        Unreal Mannequin
        UE5 Manny
        Unity Humanoid
        Generic humanoid

---

# 24. Retargeting UI

Provide:

    Retarget Animation

    Source Skeleton
    Kimodo SOMA

    Target Skeleton
    [ Unreal Manny ▼ ]

    Mapping

    Pelvis        → pelvis
    Left Hip      → thigh_l
    Right Hip     → thigh_r
    Spine         → spine
    ...

    [ Auto Map ]

    [ Preview ]

    [ Apply ]

If mapping is incorrect:

    ⚠ 3 joints require manual mapping.

Allow users to manually select bones.

---

# 25. Game Engine Export

Primary goal:

Make generated animations easy to use in game engines.

Prioritize:

1. GLB / glTF
2. FBX
3. BVH
4. Native animation data where useful

Do not implement every format immediately.

Start with:

    GLB
    glTF

Then add:

    FBX
    BVH

---

# 26. Unreal Engine Workflow

Ideal workflow:

    Text Prompt
         ↓
    Kimodo
         ↓
    Retarget
         ↓
    Unreal-compatible skeleton
         ↓
    Export
         ↓
    Import into Unreal Engine

Provide an export preset:

    Unreal Engine

    Target:
    [ UE5 Manny ]

    Format:
    [ FBX ]

    FPS:
    [ 30 ]

    Root Motion:
    [ Enabled ]

    [ Export ]

If FBX licensing/tooling makes direct implementation problematic,
provide GLB/glTF first and design the exporter interface so FBX can be
added later.

---

# 27. Unity Workflow

Provide:

    Unity Humanoid

    Target:
    [ Humanoid ]

    Format:
    [ FBX / GLB ]

    [ Export ]

The exported animation should preserve:

- bone hierarchy
- rotations
- root translation
- frame rate

---

# 28. Blender Workflow

Provide:

    Blender

    Format:
    [ GLB ]

    [ Export ]

GLB should be a first-class export format.

---

# 29. GLB Export

Use a proper glTF/GLB representation.

Export:

- skeleton
- animation
- joint hierarchy
- transforms
- FPS

If mesh export is not available:

    Skeleton-only GLB

Document that users can attach their own mesh in Blender,
Unreal, or another DCC.

---

# 30. FBX Export

Implement through a clearly separated exporter.

Do not contaminate the core animation system with FBX-specific code.

Interface:

    class AnimationExporter
    {
    public:

        virtual bool exportAnimation(
            const Animation& animation,
            const ExportOptions& options
        ) = 0;
    };

Then:

    GLBExporter
    FBXExporter
    BVHExporter

---

# 31. Export Presets

Users should not have to understand technical details.

Provide presets:

    Unreal Engine
    Unity
    Blender
    Generic

Advanced settings can expose:

- FPS
- scale
- root motion
- coordinate system
- bone orientation
- frame range

---

# 32. Coordinate Systems

This is extremely important.

Different engines use different:

- up axes
- forward axes
- handedness
- units

Create a conversion layer.

Example:

    Kimodo
        ↓
    Coordinate Converter
        ↓
    Unreal
    Unity
    Blender

Do not bake coordinate conversions into the Kimodo data.

---

# 33. Root Motion

Support:

- root translation
- in-place animation
- extracted root motion

UI:

    Root Motion

    ○ In Place
    ● Preserve Root Motion
    ○ Extract Root Motion

This will be important for game development.

---

# 34. Animation Quality Controls

Allow:

- FPS conversion
- frame range
- smoothing
- interpolation
- root-motion extraction
- optional rotation cleanup

Do not alter generated animation by default.

Default behavior should preserve Kimodo output.

---

# 35. Application Settings

Settings screen:

    General
    ─────────────────

    Language
    Theme
    Startup page


    Models
    ─────────────────

    Model directory
    Cache directory


    Performance
    ─────────────────

    GPU
    CPU threads
    VRAM limit
    Vulkan settings


    Export
    ─────────────────

    Default format
    Default FPS
    Default export directory


    Hugging Face
    ─────────────────

    Account
    Authentication

---

# 36. Theme

Create a custom dark theme.

Suggested style:

- dark graphite background
- subtle borders
- slightly rounded panels
- restrained accent color
- clear typography
- large preview area

Do not overuse gradients.

Avoid excessive neon.

The application should look professional.

---

# 37. Dear ImGui Styling

Create:

    Theme::apply();

Do not scatter:

    ImGui::PushStyleColor(...)

throughout the entire application.

Centralize style configuration.

Create reusable components:

    UI::Button()
    UI::Card()
    UI::Section()
    UI::ModelCard()
    UI::Progress()
    UI::Toast()

---

# 38. Notifications

Implement toast notifications:

    ✓ Model installed

    ✓ Animation generated

    ⚠ Vulkan memory is low

    ✕ Download failed

Do not use modal dialogs for every small error.

---

# 39. Error Handling

Every failure must produce a useful user-facing error.

Bad:

    Error -1

Good:

    Unable to download the model.

    The Hugging Face repository requires authentication.

    [ Connect Hugging Face ]

Technical details:

    View logs

---

# 40. Offline Mode

The application should still work offline after models are installed.

Offline:

    Generate animation
    View animations
    Retarget
    Export
    Change settings

Internet required for:

    Model downloads
    Hugging Face authentication
    Model updates

Do not make generation depend on network access.

---

# 41. Download Manager

Downloader requirements:

- asynchronous
- progress
- cancellation
- retry
- resume where possible
- checksum verification
- temporary files
- atomic installation

Never write incomplete files as installed models.

Use:

    model.gguf.download

then:

    verify

then:

    model.gguf

---

# 42. Model Verification

Every model should have expected hashes when available.

Verification flow:

    Download
       ↓
    SHA-256
       ↓
    Compare
       ↓
    Valid?
      / \
    Yes  No
     │    │
     ▼    ▼
   Install Error

Never silently use a corrupted model.

---

# 43. Updates

Future feature.

Model manager can show:

    SOMA RP v1.1
    Installed

    Update available

    [ Update ]

Application itself can later support:

    Check for updates

But do NOT implement auto-updating until the core application is stable.

---

# 44. Security

Important rules:

- never log HF tokens
- never commit tokens
- never hardcode credentials
- validate downloaded files
- avoid arbitrary executable downloads
- restrict model paths
- sanitize filenames
- avoid shell command construction with user-controlled input
- use safe subprocess APIs
- validate exported paths

---

# 45. Performance

Target:

- 60 FPS UI
- responsive viewport
- no UI freezing
- minimal allocations per frame
- asynchronous generation
- asynchronous file operations where appropriate

Do not optimize prematurely.

First profile.

Then optimize actual bottlenecks.

---

# 46. Threading

Suggested threads:

    Main Thread
        Raylib
        ImGui
        rendering

    Generation Thread
        Kimodo inference

    Download Thread
        network/model downloads

    Optional IO Thread
        file operations

Never access ImGui from worker threads.

Never update Raylib rendering objects from arbitrary worker threads.

Communicate through thread-safe queues/state.

---

# 47. CMake

CMake should build:

    kimodo_studio

and the required dependencies.

Targets:

    kimodo_core
    kimodo_ui
    kimodo_rendering
    kimodo_models
    kimodo_animation
    kimodo_export

Use modern CMake.

Prefer:

    target_link_libraries()

over global compiler/linker flags.

---

# 48. Dependency Management

Avoid unnecessary dependencies.

Every dependency must have a reason.

Core dependencies:

    Raylib
    Dear ImGui
    Kimodo/GGML
    JSON library
    HTTP/download library

Keep third-party dependencies isolated.

---

# 49. Packaging

Primary target:

    Windows 10/11

Secondary target:

    Linux

Application distribution should include:

    KimodoStudio.exe
    required DLLs
    assets
    runtime files

Do NOT bundle restricted model weights unless legally permitted.

Models should normally be installed separately by the user.

---

# 50. Windows Installer

Eventually create:

    Kimodo Studio Setup.exe

Installer:

    Install location
    Desktop shortcut
    Start menu shortcut

Do not automatically download several gigabytes of models during
application installation.

Instead:

    Install Application
          ↓
    Launch
          ↓
    Model Setup Wizard
          ↓
    Choose Model
          ↓
    Download

---

# 51. First Launch Experience

First launch:

    Welcome to Kimodo Studio

    Generate AI-powered motion locally.

    System:
        RTX 4060
        8 GB VRAM
        Vulkan ✓

    Recommended configuration:
        GPU motion inference
        CPU text encoding

    [ Continue ]

Then:

    Choose a model

    [ Install Model ]

---

# 52. Model Storage

Use OS-appropriate application directories.

Example Windows:

    %LOCALAPPDATA%/KimodoStudio/

Structure:

    models/
    animations/
    cache/
    logs/
    settings/
    temp/

Never assume the working directory is writable.

---

# 53. Animation Storage

Example:

    animations/
        animation-001/
            metadata.json
            animation.glb
            source.json

Keep source data so the animation can be re-exported later.

---

# 54. Import / Export

Future support:

    Import GLB
    Import BVH
    Import animation

Initially prioritize export.

---

# 55. Undo / Redo

Not necessary for v1.

Do not over-engineer this.

---

# 56. Plugin Architecture

Do NOT build a plugin system in v1.

Keep the architecture extensible but simple.

---

# 57. Testing

Tests should cover:

## Animation

- skeleton hierarchy
- frame interpolation
- playback
- serialization

## Retargeting

- joint mapping
- transform conversion
- coordinate conversion
- root motion

## Models

- installation
- verification
- invalid model handling

## Export

- valid GLB
- valid animation data
- FPS
- hierarchy
- transforms

---

# 58. CI

Eventually use GitHub Actions.

Build:

    Windows
    Linux

Run:

    build
    tests
    formatting
    static analysis

Do not require a GPU for basic CI tests.

GPU tests can be separate.

---

# 59. Logging

Example:

    [INFO] Kimodo Studio starting
    [INFO] GPU: NVIDIA RTX 4060
    [INFO] VRAM: 8192 MB
    [INFO] Vulkan initialized
    [INFO] Model loaded
    [INFO] Generation started
    [INFO] Generation finished: 150 frames

Never:

    [INFO] HF token: hf_xxxxxxxxx

---

# 60. Crash Handling

Production application should avoid hard crashes where possible.

Implement:

- graceful error handling
- useful error messages
- log files
- crash report information

Never silently swallow important errors.

---

# 61. Telemetry

Do NOT add telemetry by default.

If telemetry is ever added:

- opt-in
- clearly documented
- anonymous
- disabled by default

---

# 62. Accessibility

Support:

- keyboard navigation
- readable text
- scalable UI
- reasonable contrast

Do not rely solely on color for status.

---

# 63. Localization

Not required for v1.

However, avoid hardcoding UI text throughout deeply nested code.

Keep strings centralized enough that localization can be added later.

---

# 64. Development Phases

DO NOT implement the entire project in one pass.

Follow these phases.

---

# PHASE 0 — Repository Analysis

Before writing significant code:

1. Inspect kimodo.cpp.
2. Understand its C API.
3. Build kimodo.cpp independently.
4. Run its existing demo.
5. Identify:
   - model paths
   - model formats
   - generation API
   - skeleton representation
   - output format
   - Vulkan initialization
6. Document findings.

Do not modify Kimodo unnecessarily.

---

# PHASE 1 — Minimal Native Application

Goal:

Create:

    C++23
    CMake
    Raylib
    Dear ImGui

Application:

    window
    ImGui
    3D viewport
    camera
    grid

Success criteria:

- application starts
- 60 FPS
- ImGui works
- 3D camera works

Do NOT implement model downloading yet.

---

# PHASE 2 — Kimodo Integration

Integrate the Kimodo C API.

Implement:

    KimodoAdapter

Features:

- initialize
- load model
- unload
- generate
- error handling

Use one locally installed model.

Success criteria:

User enters:

    "A person walks forward."

Application generates an animation.

---

# PHASE 3 — Animation Viewer

Implement:

- skeleton
- bones
- joints
- animation playback
- timeline
- camera
- grid

Success criteria:

Generated animation is visible and playable.

---

# PHASE 4 — Professional UI

Build:

- sidebar
- home
- generate page
- animation library
- settings
- custom theme
- cards
- progress indicators
- notifications

Success criteria:

Application no longer looks like a developer prototype.

---

# PHASE 5 — Model Manager

Implement:

- model registry
- installed model detection
- model metadata
- model installation
- verification
- deletion
- model selection

Success criteria:

User can manage models without using a terminal.

---

# PHASE 6 — Hugging Face Integration

Implement:

- authentication
- repository access
- model download
- progress
- retry
- verification

Success criteria:

A new user can install a supported model entirely from the GUI.

---

# PHASE 7 — Low-VRAM Support

Implement:

- VRAM detection
- GPU detection
- Vulkan detection
- automatic configuration
- text encoder chunk configuration
- CPU offload where supported

Test specifically on:

    RTX 4060 8 GB

Success criteria:

The application avoids unnecessary VRAM allocation failures.

---

# PHASE 8 — Animation Library

Implement:

- save animation
- metadata
- thumbnails
- history
- delete
- rename
- reopen

---

# PHASE 9 — Retargeting

Implement generic skeleton representation.

Then:

    Kimodo SOMA
        ↓
    Generic Humanoid
        ↓
    Unreal / Unity / Blender

First implement automatic mapping.

Then manual correction.

---

# PHASE 10 — GLB Export

Implement robust GLB export.

Verify exported files in:

- Blender
- Three.js
- other glTF viewers

Success criteria:

Animation survives export correctly.

---

# PHASE 11 — Game Engine Presets

Implement:

    Unreal
    Unity
    Blender

with:

- coordinate conversion
- FPS
- scale
- root motion
- skeleton mapping

---

# PHASE 12 — FBX

Only after GLB is stable.

Investigate the best legally distributable FBX implementation/library.

Keep exporter isolated.

---

# PHASE 13 — Production Packaging

Implement:

- Windows build
- installer
- bundled dependencies
- application icon
- version information
- model setup wizard
- logs
- error handling

---

# PHASE 14 — Testing

Test on:

- RTX 4060 8 GB
- lower VRAM GPU
- integrated graphics where possible
- NVIDIA GPU
- different RAM configurations

Test:

- generation
- model installation
- authentication
- download interruption
- corrupted model
- export
- retargeting
- application restart

---

# 65. MVP Definition

The MVP should NOT include everything.

MVP:

    ✓ Raylib
    ✓ Dear ImGui
    ✓ Kimodo integration
    ✓ Vulkan
    ✓ Model selection
    ✓ Prompt input
    ✓ Generate animation
    ✓ 3D skeleton viewer
    ✓ Timeline
    ✓ Save animation
    ✓ GLB export
    ✓ Basic model installation

After MVP:

    → Hugging Face integration
    → retargeting
    → Unreal preset
    → Unity preset
    → advanced export
    → production installer

---

# 66. Definition of "Production Ready"

The application is production-ready when:

    ✓ No terminal required for normal usage
    ✓ User can install models through GUI
    ✓ Model downloads are verified
    ✓ Hugging Face authentication works
    ✓ Application handles network failures
    ✓ Application handles insufficient VRAM
    ✓ UI does not freeze during generation
    ✓ Animations can be saved
    ✓ Animations can be reopened
    ✓ GLB exports correctly
    ✓ Retargeting works
    ✓ Application can be packaged
    ✓ Restricted models are not illegally redistributed
    ✓ No credentials are leaked
    ✓ Errors are understandable
    ✓ Logs exist
    ✓ Application works offline after model installation

---

# 67. Important Engineering Rules

## Rule 1

Do not rewrite Kimodo unnecessarily.

---

## Rule 2

Do not mix UI and inference code.

---

## Rule 3

Do not block the main rendering thread.

---

## Rule 4

Do not make the application depend on an internet connection for inference.

---

## Rule 5

Do not hardcode model paths.

---

## Rule 6

Do not hardcode Hugging Face tokens.

---

## Rule 7

Do not package models unless their licenses permit redistribution.

---

## Rule 8

Do not assume 8 GB VRAM is enough for every configuration.

---

## Rule 9

Do not use default ImGui styling.

---

## Rule 10

Do not implement advanced features before the core generation pipeline works.

---

# 68. Agent Workflow

The coding agent MUST follow this order:

    1. Analyze repository
    2. Build original Kimodo
    3. Create CMake project
    4. Add Raylib
    5. Add Dear ImGui
    6. Create basic viewport
    7. Integrate Kimodo
    8. Generate one animation
    9. Render skeleton
    10. Add playback
    11. Build professional UI
    12. Add model manager
    13. Add Hugging Face
    14. Add download verification
    15. Add animation library
    16. Add retargeting
    17. Add GLB export
    18. Add engine presets
    19. Add packaging
    20. Test

At every phase:

    BUILD
    RUN
    TEST
    FIX
    COMMIT

Do not accumulate hundreds of changes without testing.

---

# 69. Git Strategy

Use commits like:

    feat: initialize raylib application

    feat: integrate dear imgui

    feat: add kimodo adapter

    feat: implement animation viewer

    feat: add model manager

    feat: add huggingface authentication

    feat: add model downloader

    feat: add animation library

    feat: add humanoid retargeting

    feat: add glb exporter

    feat: add unreal export preset

    feat: add windows packaging

Keep commits small and understandable.

---

# 70. Final Target Architecture

Final application:

                    ┌─────────────────────────────┐
                    │       Kimodo Studio         │
                    │                             │
                    │ C++                         │
                    │ Raylib                      │
                    │ Dear ImGui                  │
                    └──────────────┬──────────────┘
                                   │
             ┌─────────────────────┼─────────────────────┐
             │                     │                     │
             ▼                     ▼                     ▼
       Model Manager          Animation System      Settings
             │                     │
             ▼                     ▼
       Hugging Face            Retargeter
             │                     │
             ▼                     ▼
       Model Registry        Game Skeletons
                                   │
                                   ▼
                              Export System
                                   │
                     ┌─────────────┼─────────────┐
                     ▼             ▼             ▼
                    GLB           FBX           BVH
                     │             │             │
                     ▼             ▼             ▼
                 Blender       Unreal         Unity
                                   │
                                   ▼
                            Game Development


                   ┌─────────────────────────────┐
                   │        Kimodo Adapter       │
                   └──────────────┬──────────────┘
                                  │
                                  ▼
                   ┌─────────────────────────────┐
                   │          kimodo.cpp          │
                   │                             │
                   │ C++ / GGML / Vulkan         │
                   │                             │
                   │ Text → Motion               │
                   └─────────────────────────────┘

---

# 71. Long-Term Vision

The final product should not feel like:

    "A GUI for kimodo.cpp"

It should feel like:

    "A local AI motion-generation application."

The user should not need to know:

- GGML
- Vulkan
- GGUF
- model directories
- inference parameters
- skeleton internals

unless they open Advanced Settings.

The primary workflow should be:

    Open Kimodo Studio
          ↓
    Select / install model
          ↓
    Type:
    "A person runs and jumps over an obstacle."
          ↓
    Generate
          ↓
    Preview
          ↓
    Retarget
          ↓
    Export
          ↓
    Use in Unreal / Unity / Blender

That is the core product experience.

---

# 72. Final Priority

When deciding what to build next, prioritize:

    1. Reliability
    2. Generation correctness
    3. Performance
    4. UI/UX
    5. Model management
    6. Export compatibility
    7. Advanced features

A beautiful UI that cannot reliably generate animations is not useful.

A reliable generator with a clean UI can be progressively improved.

---

# 73. First Task For The Agent

DO NOT implement the entire plan immediately.

Start with ONLY:

    1. Clone/build kimodo.cpp.
    2. Verify the existing Kimodo demo works.
    3. Create a separate C++23 CMake application.
    4. Integrate Raylib.
    5. Integrate Dear ImGui.
    6. Create a window.
    7. Create a 3D viewport.
    8. Create a custom dark UI.
    9. Add a basic camera/grid.
    10. Build and run successfully.

Then stop and report:

    - Build result
    - Compiler
    - OS
    - GPU
    - Vulkan status
    - Raylib version
    - Dear ImGui version
    - Kimodo build status
    - Current architecture
    - Files created
    - Any blockers

Only after this milestone is confirmed should the agent proceed to
Kimodo integration.

---

# END OF PLAN
