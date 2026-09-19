# Kimodo Studio Project Rules

## Project Scope & Architecture
- **Language**: Modern C++23 (latest MSVC standard, `/std:c++23` via `CMAKE_CXX_STANDARD 23`).
- **Libraries**: Raylib 5.5, Dear ImGui (via rlImGui), GGML Vulkan backend,
  nativefiledialog (OS file pickers), cgltf (glTF loading).
  JSON/binary persistence uses hand-rolled helpers centralized in `src/utils/JsonUtils.*` —
  **no nlohmann/json**.
- **Goal**: Native high-performance AI Motion generation & retargeting studio.

## Coding Standard
The authoritative standard is `docs/CODING_STANDARD.md`. Summary:

1. **Naming**
   - Types, functions, enum types, enum values: `PascalCase` (`Animation`, `LoadCharacter()`, `Screen::Generate`).
   - Variables, members, parameters, constants: `snake_case` (`frame_count`, `bool loaded = false;`, `default_fps`).
   - No `F`/`E`/`S`/`b`/Hungarian prefixes, no camelCase identifiers. Ever.
   - Files named after their primary type: `Animation.h` / `Animation.cpp`.
2. **External types are exempt** — never rename raylib (`Vector3`, `Matrix`, `Camera3D`, `Color`),
   ImGui, cgltf, Win32 (`dw*`, `lpsz*`), or `nfd*` symbols.
3. **Namespaces** — everything lives in `namespace studio`.
4. **Hygiene** — `[[nodiscard]]` on query functions returning bool/value; `final` on leaf classes;
   `explicit` on single-argument constructors. Shared helpers (JSON, float formatting, binary
   read/write) live only in `src/utils/` — never re-declare local copies.
5. **Formatting** — enforced by the checked-in `.clang-format` (LLVM-based, 4-space indent,
   100 columns). Keep include order author-controlled (`windows.h` before `bcrypt.h`).

## Coding & UI Rules
1. **No Fake / Dummy UI**: Do not add buttons, dropdowns, or controls that have empty bodies or are purely cosmetic placeholders. Every interactive element must be wired to real backend functionality or omitted.
2. **Style Centralization**: Always reference `UIStyle` and `Theme` in `src/ui/Theme.h` for window heights, paddings, and brand colors.
3. **Typography**: Text is rendered with Roboto (`Roboto-Regular.ttf`); icons are in `studio::icons::*` (`Icons.h`, FontAwesome 6 Solid).
4. **State Machine**: Screen routing is strictly controlled via `AppState::Screen` and `AppState` UI state. Synchronize left navigation bar and right inspector tabs when adding/modifying screens.
5. **Native File Dialogs**: Use `FileDialog` (`src/utils/FileDialog.h`) for every file/folder
   picker — never OS-specific shell code, never hand-typed path fields as the only entry point.
