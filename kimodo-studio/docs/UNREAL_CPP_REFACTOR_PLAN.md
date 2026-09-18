# Kimodo Studio — Unreal Engine C++ Style Refactor Plan

> Goal: align the entire `src/` codebase (84 files, ~45 project-owned types) with
> [Epic's UE C++ Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-c-plus-plus-coding-standard-in-unreal-engine)
> while keeping the app a **plain C++23 raylib/ImGui application** — no UObject, no UE headers.
> We adopt UE's *naming and organization discipline*, not its engine machinery.

---

## 0. Current State (audited)

| Aspect | Today | UE Standard | Gap |
|---|---|---|---|
| Namespaces | `namespace studio` everywhere | Discouraged but permitted | Keep `studio` (documented deviation) |
| Class/struct names | `PascalCase`, no prefix | `F` data types, `U` objects, `A` actors, `S` widgets, `I` interfaces, `E` enums, `T` templates | **Missing prefixes** (~45 types) |
| Enum names | `enum class Screen { Home, ... }` | `enum class EScreen` (+ UE5 scoped enums) | Rename 8 enums |
| Enum values | `Screen::Home` | Namespaced values (`EScreen::Home`) | Free — already scoped via `enum class` |
| Member variables | `snake_case_` trailing underscore (`entries_`, `activeId_`) | `PascalCase` (no marker) | **Largest churn** |
| Booleans | `running_`, `showGrid`, `vulkanAvailable` | `b` prefix (`bRunning`, `bShowGrid`) | Rename all bools |
| Out-params | `std::string& error`, `LibraryEntry& out` | `Out` prefix (`FString& OutError`) | Rename at call sites |
| Accessors | `entries()`, `activeId()` | `GetEntries()`, `GetActiveId()` (UE convention) | Mechanical rename |
| Setters | `setGrid(bool)` | `SetGrid()` | Mechanical rename |
| Interfaces | `AnimationExporter` (abstract base) | `I` prefix | `IAnimationExporter` |
| UI widgets | `PageExport`, `NavRail`, `HeaderBar` | `S` prefix (Slate convention) | `SPageExport`, `SNavRail`, ... |
| Free helpers | `jsonEscape` duplicated in **5 files**, `findString` in 3, `ftoa` in 2, `writeU32` in 2; stray `appDataDir()` in `ModelManager.cpp` | Shared utility module | **Deduplicate** into `utils/` |
| Includes | `"module/File.h"` quoted, project-rooted | Same pattern in UE modules | Already compliant |
| Macros | `KIMODO_*`, `ICON_FA_*` | All-caps, prefixed | Already compliant |

**External type exceptions (do NOT rename):** raylib `Vector3`, `Matrix`, `Camera3D`, `BoundingBox`, `Color`, `Image`; ImGui types; `nfd*` C API; `cgltf` types. Document this in the rules file.

---

## 1. Target Naming Map

### 1.1 Enums — `E` prefix (8 total)

| Current | New | File |
|---|---|---|
| `Screen` | `EScreen` | `app/AppState.h` |
| `ViewportMode` | `EViewportMode` | `app/AppState.h` |
| `EngineStatus` | `EEngineStatus` | `kimodo/KimodoEngine.h` |
| `LogLevel` | `ELogLevel` | `utils/Logger.h` |
| `RootMotion` | `ERootMotion` | `export/ExportPreset.h` |
| `ModelTask` | `EModelTask` | `models/ModelManager.h` |
| `ToastKind` | `EToastKind` | `ui/Toast.h` |
| `RetargetMode` | `ERetargetMode` | `retarget/SkeletonProfile.h` |

### 1.2 Data types — `F` prefix (structs + plain classes)

| Current | New |
|---|---|
| `AppState` | `FAppState` |
| `UserSettings` | `FUserSettings` |
| `Animation` | `FAnimation` |
| `LibraryEntry` | `FLibraryEntry` |
| `ModelEntry` | `FModelEntry` |
| `CharacterEntry` | `FCharacterEntry` |
| `CharacterBone` | `FCharacterBone` |
| `CharacterSubmesh` | `FCharacterSubmesh` |
| `CharacterValidationReport` | `FCharacterValidationReport` |
| `ExportOptions` | `FExportOptions` |
| `ExportPreset`, `Mat3`, `Quat` | `FExportPreset`, `FMat3`, `FQuat` |
| `RetargetReport` | `FRetargetReport` |
| `Toast` (struct) | `FToast` |
| `GenerationParams` | `FGenerationParams` |
| `MotionResult` | `FMotionResult` |
| `SkeletonProfile` | `FSkeletonProfile` |
| `Soma30Spec` / `SomaPresentationSpec` | `FSoma30Spec` / `FSomaPresentationSpec` |
| `SkinVertex` / `SkinningData` | `FSkinVertex` / `FSkinningData` |
| `UIStyle` / `Theme` | `FUIStyle` / `FTheme` |
| `BoneMap` / `CharacterBoneMap` (aliases) | `FBoneMap` / `FCharacterBoneMap` |

### 1.3 Service classes — `F` prefix

`Application→FApplication`, `Viewport→FViewport`, `UIManager→FUIManager`, `AnimationPlayer→FAnimationPlayer`, `AnimationLibrary→FAnimationLibrary`, `CharacterLibrary→FCharacterLibrary`, `CharacterAsset→FCharacterAsset`, `ModelManager→FModelManager`, `KimodoEngine→FKimodoEngine`, `KimodoAdapter→FKimodoAdapter`, `SettingsManager→FSettingsManager`, `Logger→FLogger`, `AppPaths→FAppPaths`, `FileHash→FFileHash`, `FileDialog→FFileDialog`, `BVHParser→FBVHParser`, `CharacterLoader→FCharacterLoader`, `CharacterMapper→FCharacterMapper`, `Retargeter→FRetargeter`, `SomaPresentation→FSomaPresentation`, `Skeleton→FSkeleton`, `GridRenderer→FGridRenderer`, `SkinningRenderer→FSkinningRenderer`, `HuggingFaceClient→FHuggingFaceClient`, `HFAuthenticator→FHFAuthenticator`, `TestSuite→FTestSuite`.

### 1.4 Interfaces — `I` prefix

| Current | New |
|---|---|
| `AnimationExporter` (abstract base of BVH/GLB exporters) | `IAnimationExporter` |
| `FBVHExporter : IAnimationExporter`, `FGLBExporter : IAnimationExporter`, `FCharacterGLBExporter` | leaf exporters stay `F` |

### 1.5 UI widgets — `S` prefix (Slate convention)

`PageHome→SPageHome`, `PageGenerate→SPageGenerate`, `PageModels→SPageModels`, `PageLibrary→SPageLibrary`, `PageCharacters→SPageCharacters`, `PageRetarget→SPageRetarget`, `PageExport→SPageExport`, `PageSettings→SPageSettings`, `NavRail→SNavRail`, `HeaderBar→SHeaderBar`, `StatusBar→SStatusBar`, `TimelineBar→STimelineBar`, `Toasts→SToasts` (drawing helper).

---

## 2. Phased Execution

Each phase is one commit, must compile clean (`/W4 /permissive-`) and pass
`--selftest-all` + `--screenshot` before the next phase starts. Never mix
semantic changes with renames.

### Phase 0 — Baseline & tooling (1 commit, 0 source changes)
1. Build + run `--selftest-all`; record PASS in commit message.
2. Add `.clang-format` seeded from Epic's published UE `.clang-format`
   (4-space indent, 120 col, PascalCase-friendly). Run a formatting pass once:
   `clang-format -i $(git ls-files 'src/*.h' 'src/*.cpp')`.
3. Verify diff is whitespace-only (`git diff -w` empty ⇒ safe).

### Phase 1 — Codify the standard in `.agents/rules/kimodo-studio.md` (this doc's §1 becomes project law)
- Fix factual drift in the rules file: C++20 → **C++23**; remove the
  `nlohmann/json` claim (project uses hand-rolled JSON helpers today — Phase 6
  centralizes them); add nativefiledialog to the library list.
- Add the naming table (§1) + verification commands.

### Phase 2 — Enums → `E` prefix (8 renames, ~10 files)
- Mechanical: `Screen::` → `EScreen::` etc. Search-driven sed pass per enum,
  then compile to catch stragglers (`AppState.h`, UIManager, NavRail, all pages,
  Application, SettingsManager, KimodoEngine, Logger, exporters, ModelManager, Toast).

### Phase 3 — Types → `F` / `I` / `S` prefixes (~45 renames, all 84 files touch some)
- Order to minimize conflicts: `IAnimationExporter` first (base class), then
  `F` data structs, then `F` services, then `S` widgets.
- Method: one `sed` rule per symbol applied repo-wide, compile after each batch.

### Phase 4 — Members, bools, accessors (biggest diff; pure renames)
- Drop trailing `_`: `entries_` → `Entries`, `activeId_` → `ActiveId`.
- Bool prefix: `running_` → `bRunning`, `showGrid` → `bShowGrid`,
  `vulkanAvailable` → `bVulkanAvailable`, `installed` → `bInstalled`, ...
- Accessors: `entries()` → `GetEntries()`, `setGrid()` → `SetGrid()`,
  `load()` → `Load()`, `draw()` → `Draw()`, ... (all member functions PascalCase).
- Out-params: `error` → `OutError`, `out`/`outPath`/`outEntry` → `OutPath`/`OutEntry`.
- Unnamed bool parameters → named with `b` prefix (e.g. `Draw(bool bGrid, bool bAxes)`).

### Phase 5 — Deduplicate shared helpers (small semantic-safe consolidation)
| New file | Absorbs | Duplicated today in |
|---|---|---|
| `utils/JsonUtils.h/.cpp` | `JsonEscape`, `FindString`, `FindNumber`, `FindInt`, `FindFloat`, `FindBool` | AnimationLibrary, CharacterLibrary, SettingsManager, GLBExporter, CharacterGLBExporter, ModelManager |
| `utils/TextUtils.h` | `Ftoa` → `FloatToString` | BVHExporter, GLBExporter |
| `utils/BinaryWriter.h` | `WriteU32`, `WriteF32`, `ReadU32` | GLBExporter, AnimationLibrary |
- `ModelManager.cpp`'s private `appDataDir()` → call `FAppPaths::AppDataDir()`.
- Add `[[nodiscard]]` to query functions returning bool/value (`FindEntry`, `GetProgress`, ...).
- `final` on leaf classes (`FBVHExporter`, `SNavRail`, ...); `explicit` on single-arg ctors.

### Phase 6 — Documentation sync
- `docs/ARCHITECTURE.md`: update all type names.
- `README.md`: naming conventions note.
- New `docs/CODING_STANDARD.md` = this plan's §1 promoted to permanent doc
  (plan itself can be archived after completion).

### Optional Phase 7 — File renaming (defer; only if wanted)
- UE keeps one type per file named after the type (`FAnimation` in
  `Animation.h` is already close). Renaming files churns CMakeLists + all
  includes for zero behavior gain — recommend **skipping** unless desired.

---

## 3. Effort Estimate

| Phase | Files touched | Risk | Est. |
|---|---|---|---|
| 0 clang-format | all (whitespace) | none | 0.5h |
| 1 rules file | 1 | none | 0.5h |
| 2 enums | ~12 | low | 1h |
| 3 type prefixes | all 84 | low (mechanical) | 2-3h |
| 4 members/bools | ~50 | medium (large diff, review-heavy) | 3-4h |
| 5 dedup + hygiene | ~10 | low-medium | 1.5h |
| 6 docs | 3 | none | 0.5h |
| **Total** | | | **~9-11h** |

## 4. Verification Protocol (every phase)

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022 --config Release
.\build\windows-vs2022\Release\kimodo_studio.exe --selftest-all
.\build\windows-vs2022\Release\kimodo_studio.exe --screenshot test_screen.png
```

Renames are grep-verified with: `rg "\bOldName\b" src/` returning zero hits
before moving to the next symbol.
