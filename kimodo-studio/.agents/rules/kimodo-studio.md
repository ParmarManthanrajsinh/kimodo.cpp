# Kimodo Studio Project Rules

## Project Scope & Architecture
- **Language**: Modern C++23 (latest MSVC standard, `/std:c++23` via `CMAKE_CXX_STANDARD 23`).
- **Libraries**: Raylib 5.5, Dear ImGui (via rlImGui), GGML Vulkan backend,
  nativefiledialog (OS file pickers), cgltf (glTF loading).
  JSON/binary persistence uses hand-rolled helpers centralized in `src/utils/JsonUtils.*` —
  **no nlohmann/json**.
- **Goal**: Native high-performance AI Motion generation & retargeting studio.

## Coding Standard — Unreal Engine C++ Conventions
Follow [Epic's UE C++ Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-c-plus-plus-coding-standard-in-unreal-engine).
We adopt UE naming/organization discipline; the app remains plain C++ (no UObject/UE headers).
Full mapping lives in `docs/UNREAL_CPP_REFACTOR_PLAN.md`.

1. **Type prefixes** — every project-owned type is prefixed by role:
   - `E` → enums: `EScreen`, `EViewportMode`, `EEngineStatus`, `ELogLevel`, `ERootMotion`, `EModelTask`, `EToastKind`, `ERetargetMode`
   - `F` → data structs & service classes: `FAppState`, `FAnimation`, `FCharacterLibrary`, `FLogger`...
   - `I` → abstract interfaces: `IAnimationExporter`
   - `S` → UI widgets (Slate convention): `SPageExport`, `SNavRail`, `SHeaderBar`, `SToasts`...
2. **Member variables** — `PascalCase`, no trailing underscore: `Entries`, `ActiveId`.
   Booleans use the `b` prefix: `bRunning`, `bShowGrid`, `bVulkanAvailable`.
3. **Functions** — `PascalCase` for all member functions: `GetEntries()`, `SetGrid()`, `Draw()`, `Load()`.
   Out-parameters carry the `Out` prefix: `bool FindEntry(const std::string& Id, FCharacterEntry& OutEntry)`.
   Unnamed bool parameters are forbidden — name them with `b`: `Draw(bool bGrid, bool bAxes)`.
4. **Namespaces** — everything lives in `namespace studio` (documented deviation from UE;
   UE discourages namespaces but permits this single top-level app namespace).
5. **External types are exempt** — never rename raylib (`Vector3`, `Matrix`, `Camera3D`, `Color`),
   ImGui, cgltf, or `nfd*` symbols.
6. **Hygiene** — `[[nodiscard]]` on query functions returning bool/value; `final` on leaf classes;
   `explicit` on single-argument constructors. Shared helpers (JSON, float formatting, binary
   read/write) live only in `src/utils/` — never re-declare local copies.

## Coding & UI Rules
1. **No Fake / Dummy UI**: Do not add buttons, dropdowns, or controls that have empty bodies or are purely cosmetic placeholders. Every interactive element must be wired to real backend functionality or omitted.
2. **Style Centralization**: Always reference `FUIStyle` and `FTheme` in `src/ui/Theme.h` for window heights, paddings, and brand colors.
3. **Typography**: Text is rendered with Roboto (`Roboto-Regular.ttf`); icons are in `studio::icons::*` (`Icons.h`, FontAwesome 6 Solid).
4. **State Machine**: Screen routing is strictly controlled via `FAppState::Screen` (`EScreen`) and `FAppState::LastToolScreen`. Synchronize left navigation bar and right inspector tabs when adding/modifying screens.
5. **Native File Dialogs**: Use `FFileDialog` (`src/utils/FileDialog.h`) for every file/folder
   picker — never OS-specific shell code, never hand-typed path fields as the only entry point.
6. **Build & Test**:
   - MSBuild path: `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`
   - Verification command: `.\build\windows-vs2022\Release\kimodo_studio.exe --screenshot test_screen.png`
   - Full check per commit: clean Release build (0 errors/warnings) + `--selftest-all` all green.
