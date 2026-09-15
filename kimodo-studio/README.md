# Kimodo Studio

Local AI motion generation: text prompt → skeletal animation → retarget → GLB/BVH export.

## Build (Windows)

```powershell
cd kimodo-studio
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022
```

Requires the Kimodo `build/Release` artifacts (MSVC `kimodo.lib` + ggml);
the root `E:/kimodo.cpp/build` tree provides them. Raylib/ImGui/rlImGui
arrive via pinned FetchContent. Root Kimodo sources stay untouched except
for the additive, backward-compatible diffusion progress callback.

## Run

```powershell
.\build\windows-vs2022\Release\kimodo_studio.exe
kimodo_studio.exe --version
kimodo_studio.exe --selftest [frames] [steps]   # backend + viewer + retarget + library
kimodo_studio.exe --selftest-hf                 # auth + download pipeline
kimodo_studio.exe --selftest-export [keepPath]  # GLB/BVH round-trip + presets
```

## First launch (Model Setup Wizard flow)

1. Open **Models**. No model installed → status shows missing.
2. Either **Download** from Hugging Face (Settings → connect token first,
   public repos work without one) or **Import** a local `.gguf`.
3. Every install verifies SHA-256 before atomic placement.
4. Go to **Generate**, type a prompt, Generate. Finished clips auto-save
   into the **Library** with thumbnails.

Models are never bundled with the app (license). They live in
`%LOCALAPPDATA%/KimodoStudio/models` (plus the dev checkout path).

## Retargeting

Target: Blender Generic (default). Direct local copy, stable.
Source: current animation. Mapping: Auto Map. Preview / Apply.

- **Blender generic** — first-class, direct local copy. No UE logic.
- **Unity Humanoid** — GenericLocal rotation transfer.
- **UE5 Manny (deprecated)** — internal chain/IK path kept for compat
  only. Do NOT use for new work.

New architecture: Export motion from Kimodo, use Unreal Engine IK
Retargeter for final UE skeleton retargeting. See
`docs/unreal-bvh-workflow.md`.

## Export

Library → Export: Format BVH, Preset Humanoid, FPS, Scale, Root Motion,
Rotation XYZ. Main UE pipeline = BVH Humanoid export.

BVH: Y-up right-handed meters, XYZ Euler degrees, ROOT translation +
rotation, children rotation only. Old Unreal preset deprecated, kept
for compat. GLB stable path preserved. FBX not vendored — convert via
Blender/Unreal/Unity.

## Package

```powershell
cmake --build --preset windows-vs2022
cpack   # run inside build/windows-vs2022 → portable zip: exe + DLLs + config
```

## Layout

`src/app` shell · `src/ui` screens/theme/toasts · `src/rendering` viewport ·
`src/kimodo` C-API adapter + worker · `src/animation` data + player ·
`src/library` persistence · `src/models` registry/install/verify ·
`src/huggingface` auth/downloads · `src/retarget` profiles ·
`src/export` GLB/BVH + engine presets.
