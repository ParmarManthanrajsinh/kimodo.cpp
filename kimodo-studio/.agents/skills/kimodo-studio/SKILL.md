---
name: kimodo-studio
description: Expert agent for the Kimodo Studio AI Motion Workstation and kimodo.cpp engine. Use when modifying the UI, 3D viewport, SOMA retargeting, diffusion inference runtime, animation player, exporters (BVH/GLB), or build system.
---

# Kimodo Studio & Kimodo.cpp Expert Guide

Kimodo Studio is a native, high-performance desktop workstation for AI character motion generation and skeleton retargeting. It is built in modern C++ (C++20/C++latest) using Raylib 5.5, Dear ImGui, rlImGui, and the GGML Vulkan tensor library.

---

## 1. Architecture Overview

### Core Subsystems (`src/`)
- **Application (`src/app/`)**: Main lifecycle (`Application.cpp`), application state (`AppState.h`), frame loop, headless capture mode (`--screenshot <file>`).
- **Kimodo Engine (`src/kimodo/`)**: Adapter (`KimodoAdapter.cpp`) bridging `kimodo.cpp` core diffusion engine with the UI worker thread.
- **Animation System (`src/animation/`)**:
  - `Animation.h`: Joint rotations (quaternions) + root translations per frame.
  - `AnimationPlayer.h`: Real-time playback, loop, scrubbing, FPS scaling, forward kinematics.
  - `Skeleton.h`: SOMA-30 specification (30 joints), rest poses, forward kinematics.
- **Retargeting (`src/retarget/`)**:
  - `Retargeter.h`: Automated & manual joint mapping from SOMA-30 to target rigs (Blender generic, Mixamo, Unreal Mannequin, VRoid).
  - `SkeletonProfile.h`: Built-in profiles and custom profile JSON serialization.
- **Rendering & Viewport (`src/rendering/`)**:
  - `Viewport.h`: 3D arcball camera, skeleton bone visualizer, joint spheres, coordinate grid (`GridRenderer.cpp`), raylib 3D integration.
- **UI Architecture (`src/ui/`)**:
  - `UIManager.h/cpp`: Main dock layout, top header bar, left navigation rail (`WORKSPACE`, `ASSETS`, `SYSTEM`), active tool viewports, right tool/inspector panels, bottom timeline + transport scrubber, and status bar.
  - `Theme.h/cpp`: Centralized UI style tokens (`UIStyle`), color palettes, custom vector Kimodo logo.
  - `Toast.h/cpp`: Non-intrusive notification queue, top-layered ImGui card rendering.
  - `Icons.h`: FontAwesome 6 Solid UTF-8 icon glyphs (`kHome`, `kGenerate`, `kRetarget`, `kCheck`, etc.).
- **Export & Import (`src/export/`)**:
  - `BVHExporter.h`: Biovision Hierarchy text motion export.
  - `GLBExporter.h`: Binary glTF 2.0 animated skinned mesh / node hierarchy export.
- **Model Registry & HuggingFace (`src/models/`, `src/huggingface/`)**:
  - `ModelManager.h`: Local/remote model discovery, checksums, download tasks.
  - `HFAuthenticator.h`: Hugging Face Hub token validation via WinHTTP / curl.

---

## 2. Coding Guidelines & Rules

### Modern C++ Standards
- Use modern C++ features (`std::span`, `std::filesystem`, structured bindings).
- Keep header dependencies light; use forward declarations where possible.
- Wrap UI logic with clear state ownership inside `AppState`.

### UI & UX Principles
- **No Placeholder / "Just For Show" Elements**: Every button, pill, toggle, or control must have actual logic behind it. Avoid dummy empty `{}` handlers.
- **Dynamic Layout & Sizing**: Use `UIStyle` constants (`UIStyle::topH`, `UIStyle::sideW`, `UIStyle::timelineH`, `UIStyle::statusH`). Never hardcode magic layout offsets across multiple files.
- **Typography & Icons**: Plain text uses Roboto font (`fonts/Roboto-Regular.ttf`); icons use FontAwesome (`fonts/fa-solid-900.ttf` mapped via `studio::icons::*`).
- **Z-Order & Layering**: Toasts and modal overlays must float above all docked panels using `ImGui::BringWindowToDisplayFront()` and proper base height offsets.

---

## 3. Build & Test Commands

### Visual Studio 2022 (MSVC x64)
Fast multi-core compilation:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" build\windows-vs2022\kimodo_studio.vcxproj /p:Configuration=Release /m
```

### Headless Verification & Screenshots
Run instant headless frame capture to verify UI rendering without launching a desktop window:
```powershell
.\build\windows-vs2022\Release\kimodo_studio.exe --screenshot test_screen.png
```

### Git & Source Cleanliness
- Treat warnings as errors on studio code.
- Ensure all resources copied by CMake post-build (`fonts/`, `version.rc`, icons) remain synchronized.
