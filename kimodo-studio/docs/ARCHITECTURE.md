# Kimodo Studio — Technical Architecture

Kimodo Studio is a native C++23 desktop motion generation and animation workstation built on **Raylib 5.5**, **Dear ImGui (Docking)**, and **kimodo.cpp** (native ggml/C++ inference runtime for SOMA diffusion motion models).

---

## 1. High-Level Pipeline & Architecture

```
Kimodo Diffusion (ggml runtime)
             ↓
     Animation Data (SOMA30)
             ↓
  SOMA Presentation Layer (58 Joints)
             ↓
   Character System & GPU/CPU Skinning  ←  glTF/GLB Character Mesh (CesiumMan.glb)
             ↓
       3D Viewport (Raylib + ImGui)
             ↓
     Export Pipelines
     ├── Blender Generic GLB / BVH  ──→  Blender DCC
     └── Generic Humanoid BVH      ──→  Unreal Engine (IK Rig + IK Retargeter → Manny/MetaHuman)
```

### Unreal Engine Integration Strategy
Kimodo Studio **does not** perform custom in-engine IK solves, fake Manny bind poses, or pelvis/foot IK reconstruction.
- **Kimodo Studio output**: Clean, deterministic generic humanoid BVH (`bvh-humanoid` preset) with configurable FPS, frame range, root motion, and standard Euler XYZ rotations.
- **Unreal Engine pipeline**: Import BVH → Setup Source IK Rig → Retarget using Unreal's native **IK Retargeter** → Target Manny / Quinn / MetaHuman.

### Blender Integration Strategy
- **Blender Generic**: First-class direct local rotation retargeting with standard Blender humanoid bone naming conventions.
- Direct BVH or Skeleton GLB export for seamless drag-and-drop into Blender.

---

## 2. Core Subsystems

### A. SOMA Presentation Skeleton (`src/animation/`)
- **Base SOMA Model**: 30-joint representation output by diffusion model.
- **Presentation Layer (`SomaPresentation`)**: Expands SOMA30 to a standard 58-joint presentation hierarchy with:
  - Pelvis / Spine hierarchy (Spine1, Spine2, Spine3, Neck, Head)
  - Limbs with clavicles, shoulders, elbows, wrists
  - Hand metacarpals and full 5-finger hierarchies per hand
  - Feet with ankles, balls, and toe ends
- Validates hierarchy topological ordering, left/right symmetry, and finite transform constraints.

### B. Character & Skinning Engine (`src/character/`, `src/rendering/`)
- **glTF/GLB Character Loader (`CharacterLoader`)**:
  - Leverages bundled `cgltf` parser.
  - Extracts skinned meshes, submeshes, PBR material factors, base color textures, bone hierarchies, and Inverse Bind Matrices (IBMs).
  - Validates vertex skin weights (up to 4 influences per vertex), height, and humanoid bone groups.
- **Bone Mapper (`CharacterMapper`)**:
  - Auto-maps character bones across Mixamo, Blender, glTF, and Unreal naming conventions.
  - Evaluates per-frame bone skin matrices: $M_{\text{skin}} = M_{\text{invBind}} \cdot M_{\text{world}}$.
- **Skinning Renderer (`SkinningRenderer`)**:
  - **GPU Hardware Skinning**: Custom GLSL 330 shader transforming vertex positions and normals on GPU.
  - **CPU Skinning Fallback**: Multi-threaded SIMD-friendly CPU skinning updating dynamic vertex buffers when shaders are unavailable or disabled.
  - Directional 3-point studio lighting and material texture sampling.

### C. 3D Viewport (`src/rendering/Viewport.h`)
- Integrated Raylib 3D viewport rendered directly to texture for Dear ImGui canvas.
- Display Modes: `[ Character ]`, `[ Skeleton ]`, `[ Both ]`.
- Visual Features:
  - Wireframe overlay mode.
  - 3D Billboard bone labels in world space.
  - Anti-aliased infinite grid, coordinate axes, and shadow receiver floor plane.
  - Orbit camera with framing (`F`), zoom, and smooth damping.

### D. BVH Exporter & Parser (`src/export/`)
- **BVH Exporter (`BVHExporter`)**:
  - Standards-compliant Y-up, right-handed, meters, XYZ Euler degrees.
  - `ROOT` with translation and rotation channels; `JOINT` with rotation channels.
  - Deterministic quaternion-to-Euler XYZ decomposition avoiding gimbal locks.
  - Configurable FPS (24, 30, 60), root motion preservation/locking, and frame range export.
- **BVH Parser (`BVHParser`)**:
  - Full ASCII BVH parser with hierarchy tree reconstruction.
  - Euler XYZ to Quaternion synthesis.
  - Strict validation of hierarchy acyclicity, joint naming, channel counts, and frame consistency.
  - Enables lossless round-trip validation: `Animation -> BVH -> Animation`.

### E. Character GLB Exporter (`src/export/CharacterGLBExporter.h`)
- Exports full character assets (Mesh + Skinning + Textures + Animated Skeleton) directly to standard glTF 2.0 binary (`.glb`).
- Embeds node hierarchy, IBM accessors, vertex attributes (`POSITION`, `NORMAL`, `TEXCOORD_0`, `JOINTS_0`, `WEIGHTS_0`), and animation samplers (`translation`, `rotation`).

### F. Dynamic Path Discovery & Settings (`src/utils/AppPaths.h`, `src/app/SettingsManager.h`)
- Zero hardcoded developer paths (`E:/...`, `C:/...`).
- Exe-relative base directory detection with `%LOCALAPPDATA%/KimodoStudio` user state directory.
- JSON persistence for viewport preferences, active character, export presets, loop/speed playback settings.

### G. Modular UI Architecture (`src/ui/`)
- Split into reusable components and focused page controllers:
  - Components: `HeaderBar`, `NavRail`, `TimelineBar`, `StatusBar`, `Toast`.
  - Pages: `PageHome`, `PageGenerate`, `PageCharacters`, `PageRetarget`, `PageExport`, `PageLibrary`, `PageModels`, `PageSettings`.

---

## 3. Included Sample Assets & Licensing

### Cesium Man (`assets/characters/CesiumMan.glb`)
- Source: Khronos glTF Sample Assets.
- Created by: Cesium GS, Inc.
- License: Creative Commons Attribution 4.0 International (CC-BY 4.0).
- Permitted Use: Commercial and non-commercial distribution with attribution.

---

## 4. Verification & Testing

Run all automated selftests from CLI:
```powershell
.\build\windows-vs2022\Release\kimodo_studio.exe --selftest-all
```

Individual test suites:
- `--selftest-bvh`: BVH export, parser, hierarchy validator, and round-trip tests.
- `--selftest-character`: Character loader, skinning data, bone mapping, and CPU skinning.
- `--selftest-soma`: SOMA presentation skeleton expansion (58 joints) and symmetry validation.
- `--selftest-blender`: Blender Generic retargeting and bone map validation.
- `--screenshot <path>`: Headless frame render and screenshot capture.
