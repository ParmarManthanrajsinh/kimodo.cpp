---
name: distribution
description: Release engineering and distribution agent for Kimodo Studio. Use when building production release packages, creating portable ZIPs or installers, validating runtime DLL dependencies, calculating release checksums, generating release notes, or preparing GitHub releases.
---

# Kimodo Studio Distribution & Release Agent Guide

This agent manages the release engineering, validation, packaging, and distribution lifecycle for **Kimodo Studio** and the `kimodo.cpp` runtime.

---

## 1. Distribution Specifications

- **Target OS**: Windows 10 / Windows 11 (64-bit).
- **Architecture**: x86_64 / AMD64.
- **Compiler Toolchain**: MSVC v143 (Visual Studio 2022 C++23).
- **Target Type**: Standalone portable deployment (`ZIP`) with bundled DLLs, fonts, assets, and configs.
- **Backend**: GGML Vulkan (`ggml-vulkan.dll`) with CPU fallback (`ggml-cpu.dll`).
- **Legal & License Protection**: Restrictive model weights (~1.1 GB motion GGUF, ~15 GB LLM2Vec text bundle) must **never** be hard-bundled in base application packages. Models are fetched on-demand or imported via the GUI.

---

## 2. Automated Pipeline (One-Command Packaging)

To build, verify, audit, hash, and stage the complete release distribution:

```powershell
powershell -ExecutionPolicy Bypass -File tools\package_release.ps1
```

### Pipeline Actions:
1. Compiles MSVC x64 `Release` target (`kimodo_studio.exe`).
2. Runs the full self-test suite (`--selftest-all`). Fails fast if any regression occurs.
3. Invokes CPack to generate `KimodoStudio-<version>-<git-hash>-windows-x64.zip`.
4. Performs an automated archive audit verifying all 10 required payload files.
5. Computes SHA-256 checksum and writes `<archive>.sha256`.
6. Generates `dist/RELEASE_NOTES.md` with package specs, SHA-256, and system requirements.

---

## 3. Required Package Payload Audit

Every release archive **must** contain exactly these artifacts:

| Item | Path in Archive | Purpose |
| :--- | :--- | :--- |
| **Main Executable** | `kimodo_studio.exe` | Main desktop application binary |
| **Vulkan GGML DLL** | `ggml-vulkan.dll` | GPU tensor compute backend |
| **CPU GGML DLL** | `ggml-cpu.dll` | CPU tensor compute backend |
| **Base GGML DLL** | `ggml-base.dll` | Core tensor memory engine |
| **GGML C API DLL** | `ggml.dll` | GGML public export table |
| **UI Font** | `fonts/Roboto-Regular.ttf` | Dear ImGui text renderer |
| **Icon Font** | `fonts/fa-solid-900.ttf` | FontAwesome 6 icon glyphs |
| **Model Registry** | `config/models.json` | Approved models & download URLs |
| **Default Character** | `assets/characters/CesiumMan.glb` | 3D mesh for skinned animation preview |
| **Documentation** | `README.md` | User documentation & usage guide |

---

## 4. Manual Distribution Steps

If running without the automated script:

### Step 1: Verification Test Run
```powershell
.\build\windows-vs2022\Release\kimodo_studio.exe --selftest-all
```
Must exit with code `0`.

### Step 2: CPack Invocation
```powershell
cmake --build build/windows-vs2022 --config Release --target package
```

### Step 3: Compute Checksum
```powershell
Get-FileHash build\windows-vs2022\KimodoStudio-*.zip -Algorithm SHA256
```

---

## 5. End-User Prerequisites (Release Notes Disclosure)

Always disclose these requirements in distribution channels:

1. **OS**: Windows 10 (Build 19041+) or Windows 11 64-bit.
2. **Visual C++ Runtime**: Microsoft Visual C++ 2015–2022 Redistributable (x64) installed.
3. **GPU Drivers**: Modern NVIDIA, AMD, or Intel GPU driver supporting Vulkan 1.2+.
4. **Model Installation**: On first launch, user navigates to **Models** page and clicks **Download from Hugging Face** or imports their local `.gguf` motion weights.
