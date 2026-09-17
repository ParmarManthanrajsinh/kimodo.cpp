# Kimodo Studio

**Kimodo Studio** is a native C++23 desktop motion-generation and animation workstation powered by **Raylib 5.5**, **Dear ImGui**, and **kimodo.cpp** (native ggml inference runtime for SOMA diffusion motion models).

---

## Key Features

- **Text-to-Motion Generation**: Direct ggml-based diffusion inference generating high-fidelity humanoid motion clips.
- **SOMA Presentation Layer**: 58-joint expanded presentation skeleton supporting full hands, fingers, and toe ends.
- **Real Character System**: glTF/GLB character loader with GPU hardware skinning (and multi-threaded CPU fallback) and PBR materials. Bundled with default **CesiumMan.glb** (CC-BY 4.0).
- **Pro 3D Viewport**: Character / Skeleton / Both display modes, wireframe toggle, 3D bone billboard names, infinite floor grid, and orbit camera controls.
- **Production BVH Pipeline**: Export generic humanoid animations in BVH with deterministic XYZ Euler angles, configurable FPS, root motion preservation, and round-trip BVH parser validation.
- **Unreal Engine Direct Workflow**: Kimodo -> Generic Humanoid BVH -> Unreal IK Rig -> Unreal IK Retargeter -> Manny/MetaHuman.
- **Blender Generic Retargeting**: First-class direct local rotation transfer to standard Blender humanoid rigs.
- **Full Character GLB Exporter**: Export complete character meshes with embedded skinning, textures, and animated skeletal tracks.
- **Dynamic Path Discovery**: Zero hardcoded machine paths; portable exe-relative assets discovery with persistent JSON user settings.

---

## Build (Windows)

```powershell
cd kimodo-studio
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022 --config Release
```

Requires MSVC with C++23 support (`/std:c++latest`) and kimodo runtime libraries. Raylib 5.5 and Dear ImGui are fetched automatically.

---

## Run & Verification

Launch the application:
```powershell
.\build\windows-vs2022\Release\kimodo_studio.exe
```

Run comprehensive test suites:
```powershell
.\build\windows-vs2022\Release\kimodo_studio.exe --selftest-all
```

Individual test flags:
```powershell
.\build\windows-vs2022\Release\kimodo_studio.exe --selftest-bvh         # BVH Exporter/Parser round-trip
.\build\windows-vs2022\Release\kimodo_studio.exe --selftest-character   # glTF character loader & skinning
.\build\windows-vs2022\Release\kimodo_studio.exe --selftest-soma        # SOMA 58-joint presentation skeleton
.\build\windows-vs2022\Release\kimodo_studio.exe --selftest-blender     # Blender generic retargeting
.\build\windows-vs2022\Release\kimodo_studio.exe --screenshot out.png   # Headless screenshot capture
```

---

## Pipelines & DCC Integration

### Unreal Engine Pipeline
1. Generate motion or select a clip from the **Library**.
2. Go to **Export** -> Format: **BVH (bvh-humanoid)**.
3. Import the BVH file into Unreal Engine 5 as an Animation Sequence (with imported source skeleton).
4. Create an **IK Rig** for the BVH skeleton and an **IK Rig** for UE5 Manny / Quinn / MetaHuman.
5. Create an **IK Retargeter** connecting the source and target IK Rigs to map motion natively in Unreal Engine.

### Blender Pipeline
1. In the **Retarget** page, select **Blender Generic** profile to preview motion on a standard Blender skeleton, or export directly to **BVH** / **GLB**.
2. Import the BVH / GLB directly into Blender.

---

## Asset Licenses & Attribution

- **Cesium Man**: `assets/characters/CesiumMan.glb` from Khronos glTF Sample Assets. Copyright © Cesium GS, Inc. Licensed under [Creative Commons Attribution 4.0 International (CC-BY 4.0)](https://creativecommons.org/licenses/by/4.0/).
- **Font Awesome Free**: `fonts/fa-solid-900.ttf` by Fonticons, Inc. (CC BY 4.0 / SIL OFL 1.1).
- **Roboto**: `fonts/Roboto-Regular.ttf` by Christian Robertson (Apache 2.0).

---

## Technical Documentation

See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for full subsystem details, skinning mathematics, coordinate conventions, and class structures.
