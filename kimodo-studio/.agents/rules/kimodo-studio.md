# Kimodo Studio Project Rules

## Project Scope & Architecture
- **Language**: Modern C++ (C++20 / latest MSVC standard).
- **Libraries**: Raylib 5.5, Dear ImGui (via rlImGui), GGML Vulkan backend, nlohmann/json.
- **Goal**: Native high-performance AI Motion generation & retargeting studio.

## Coding & UI Rules
1. **No Fake / Dummy UI**: Do not add buttons, dropdowns, or controls that have empty bodies or are purely cosmetic placeholders. Every interactive element must be wired to real backend functionality or omitted.
2. **Style Centralization**: Always reference `studio::UIStyle` and `studio::Theme` in `src/ui/Theme.h` for window heights, paddings, and brand colors.
3. **Typography**: Text is rendered with Roboto (`Roboto-Regular.ttf`); icons are in `studio::icons::*` (`Icons.h`, FontAwesome 6 Solid).
4. **State Machine**: Screen routing is strictly controlled via `AppState::screen` and `AppState::lastToolScreen`. Synchronize left navigation bar and right inspector tabs when adding/modifying screens.
5. **Build & Test**:
   - MSBuild path: `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`
   - Verification command: `.\build\windows-vs2022\Release\kimodo_studio.exe --screenshot test_screen.png`
