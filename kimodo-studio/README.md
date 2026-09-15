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

Retarget screen maps SOMA motion onto game skeletons with the standard
rest-offset formulation (`target_world = source_world * target_rest`,
root keeps source world so facing/travel match the source clip):

- **Manny Mixamo (UE)** — true 25-joint core profile extracted from a real
  FBX (names, parents, offsets, rest orientations). Fingers, twist bones
  and end nubs excluded (no SOMA source joints).
- **Unity Humanoid**, **Blender generic** — rotation transfer profiles.
- Unmapped joints hold rest. Root motion: preserve / in-place / extract.

Old `[unreal-manny]` saves from before the true profile are heap-broken
(wrong math baked in) — regenerate them.

## Export

Library → Export opens a modal: preset (Blender / Unity / Unreal /
Generic), format (GLB / BVH), FPS resample, unit scale, root motion.
Unreal preset converts to Z-up centimeters with the Manny profile.
GLB files import verified in Blender 4.1 (objects, actions, fcurves,
root travel). BVH imports as a 30-bone armature. FBX is intentionally
not vendored (proprietary SDK) — convert via Blender/Unreal/Unity.

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
