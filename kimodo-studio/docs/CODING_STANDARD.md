# Kimodo Studio — C++ Coding Standard

This is the authoritative coding standard for the Kimodo Studio codebase.
All new code and refactored code must follow it. The goal is a clean,
modern C++23 codebase built on Raylib + Dear ImGui + kimodo.cpp — readable,
predictable, and free of borrowed conventions from other engines.

---

## 1. Naming Rules

| Element        | Convention  | Examples |
|----------------|-------------|----------|
| Types          | `PascalCase` | `Animation`, `CharacterAsset`, `AnimationPlayer` |
| Functions      | `PascalCase` | `LoadCharacter()`, `UpdateSkinning()`, `FindBoneIndex()` |
| Variables      | `snake_case` | `frame_count`, `root_positions`, `file_path` |
| Members        | `snake_case` | `bool loaded = false;`, `float target_fps = 30.0f;` |
| Parameters     | `snake_case` | `void LoadAnimation(const std::string& file_path);` |
| Constants      | `snake_case` | `constexpr float default_fps = 30.0f;` |
| Enum types     | `PascalCase` | `enum class Screen` |
| Enum values    | `PascalCase` | `Screen::Generate` |
| Namespaces     | lowercase    | `namespace studio` |
| Files          | `PascalCase`, matching the primary type | `Animation.h` / `Animation.cpp` |

Forbidden:

- `F` type prefixes (`FAnimation` → `Animation`)
- `E` enum prefixes (`EScreen` → `Screen`)
- `b` boolean prefixes (`bLoaded` → `loaded`)
- `S` UI-class prefixes (`STimelineBar` → `TimelineBar`)
- Hungarian notation and `m_`-style member prefixes
- camelCase variables, parameters, or members

Acronyms inside `snake_case` names are split: `characterID` → `character_id`.
Acronyms inside `PascalCase` names are preserved as words: `LoadGLB()`,
`BVHParser`, `UIManager`.

Short loop names (`i`, `j`, `x`, `y`, `z`) are acceptable in tiny
mathematical scopes only. Persistent state must use descriptive names.

---

## 2. Formatting

Formatting is enforced by the checked-in `.clang-format` (LLVM-based,
C++23, 4-space indent, 100-column limit, LF line endings).
Run it before committing:

```bash
clang-format -i <changed files>
```

Include order is author-controlled (no automatic sorting): keep
platform headers in a dependency-safe order (e.g. `windows.h` before
`bcrypt.h`).

---

## 3. Modern C++ Usage

- C++23 (`CMAKE_CXX_STANDARD 23`).
- Prefer `std::string_view`, `std::span`, `std::optional`, `std::variant`,
  `std::array`, `std::vector`, `std::unique_ptr`.
- `std::shared_ptr` only when ownership genuinely requires it.
- `constexpr` / `consteval` where they improve clarity.
- `enum class` for all enumerations.
- Structured bindings and range-based loops.
- `[[nodiscard]]` on functions whose result must not be ignored.
- `noexcept` where justified (move operations, swap, small helpers).
- Do not add template metaprogramming unless it earns its complexity.
- Readability beats cleverness.

---

## 4. Ownership & RAII

- Every resource has exactly one clear owner.
- Raylib resources (`Texture2D`, `Model`, `RenderTexture`, `Shader`, `Mesh`)
  get explicit ownership through the object that loads them; unload happens
  in the owning destructor or explicit `Unload()` on the owner — never
  scattered through UI code.
- No raw owning pointers, no global mutable state, no singletons except
  where a subsystem genuinely requires one (`Logger`, `SettingsManager`).
- Dependencies are passed explicitly (e.g. `Application` wires
  `Viewport`, `AnimationLibrary`, `CharacterLibrary`, `ModelManager`).

---

## 5. Const Correctness

- Read-only access takes `const&`: `const Animation& animation`,
  `const std::string& file_path`.
- Use `const auto&` in range-based loops.
- Mark member functions `const` when they do not mutate state
  (`bool IsPlaying() const`, `float GetDuration() const`).
- Do not create mutable references where read-only access suffices.

---

## 6. Class Responsibilities

One class, one job. Core divisions:

| System | Responsibility |
|--------|----------------|
| `Application` | composition, frame loop, wiring |
| `AppState` | UI-facing application state |
| `Animation` | animation data (frames, joints, topology) |
| `AnimationPlayer` | playback state machine and pose evaluation |
| `Skeleton` | forward kinematics |
| `SomaPresentation` | SOMA30 → 58-joint expansion and validation |
| `CharacterAsset` | character data (mesh, skin, bones) |
| `CharacterLoader` | glTF/GLB character loading + validation |
| `CharacterMapper` | bone name mapping between skeletons |
| `Retargeter` | retargeting animation across skeletons |
| `SkinningRenderer` | GPU/CPU character skinning and rendering |
| `Viewport` | viewport state, camera, 3D drawing coordination |
| `BVHExporter` / `BVHParser` / `GLBExporter` / `CharacterGLBExporter` | export/import |
| `UIManager` + pages/components | ImGui composition only |

Do not create abstractions purely to add files.

---

## 7. Layering

- UI consumes core systems; core systems never know about UI.
  Correct: `UI → AnimationLibrary → Animation`.
  Forbidden: `Animation → UIManager`.
- Dear ImGui code lives only under `src/ui/`.
- Raylib rendering lives under `src/rendering/`; data structures
  (`Animation`, `CharacterAsset`, `Skeleton`) stay render-free.
- Data structures must not depend on ImGui or Raylib types where avoidable.

---

## 8. Error Handling

- No exceptions for expected runtime failures.
- Expected failures return `bool`, `std::optional`, or fill an
  error/result structure (e.g. `CharacterValidationReport`).
- Always surface error messages (`Logger::Error`, validation reports) —
  never silently swallow failures.
- Validate external input (files, glTF data, BVH text) before use.

---

## 9. Comments

- Explain **why**, not what. Obvious code needs no comment.
- Example: `// Kimodo produces a fixed SOMA30 topology, so preserve its
  ordering throughout the Studio animation representation.`
- No references to other engines' coding conventions, old architectures,
  or previous implementation plans.

---

## 10. Testing Conventions

Tests live in `src/tests/TestSuite.cpp` and follow the same standard:

```cpp
bool TestAnimation();
bool TestSkeleton();
bool TestBVHRoundTrip();
bool TestCharacterSkinning();
bool TestBlenderRetargeting();
```

Local naming inside tests: `animation`, `character`, `frame_index`,
`joint_index`, `expected`, `actual`. Each test returns a pass/fail `bool`
and reports details through the standard test output. Run with:

```bash
./build/windows-vs2022/Release/kimodo_studio.exe --selftest
```

---

## 11. Behavior Stability

This codebase is a motion-generation workstation: the Kimodo inference
pipeline, SOMA30 output, playback, character skinning, retargeting, and
BVH/GLB export behavior are release-critical. Refactors must preserve
behavior exactly. If a genuine bug is discovered while refactoring,
document it separately instead of silently changing behavior.

---

## 12. Quick Checklist

- [ ] Types / functions / enums: `PascalCase`
- [ ] Variables / members / parameters / constants: `snake_case`
- [ ] No `F` / `E` / `b` / `m_` / Hungarian prefixes
- [ ] Files named after their primary type
- [ ] `clang-format` clean
- [ ] RAII ownership, no global mutable state
- [ ] `const` correctness
- [ ] UI/ImGui and Raylib code out of core systems
- [ ] Errors reported, never swallowed
- [ ] `--selftest` passes before commit
