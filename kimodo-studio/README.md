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
arrive via pinned FetchContent.

## Run

```powershell
.\build\windows-vs2022\Release\kimodo_studio.exe
kimodo_studio.exe --version
kimodo_studio.exe --selftest [frames] [steps]
kimodo_studio.exe --selftest-hf
kimodo_studio.exe --selftest-export [keepPath]
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

## Package

```powershell
cmake --build --preset windows-vs2022
cpack --preset windows-vs2022   # portable zip: exe + DLLs + config
```

## Layout

`src/app` shell · `src/ui` screens/theme/toasts · `src/rendering` viewport ·
`src/kimodo` C-API adapter + worker · `src/animation` data + player ·
`src/library` persistence · `src/models` registry/install/verify ·
`src/huggingface` auth/downloads · `src/retarget` profiles ·
`src/export` GLB/BVH + engine presets.
