# Kimodo Studio Release Notes (v1.1.0)

**Kimodo Studio** is a native C++23 desktop motion-generation and animation workstation powered by Raylib 5.5, Dear ImGui, and kimodo.cpp (native ggml inference runtime for SOMA diffusion motion models).

---

## Highlights

- **All-in-One Automated Setup Wizard**: First launch automatically detects missing neural assets, authenticates with Hugging Face using secure Windows Credential Manager storage, downloads both the SOMA motion model and the complete 35-file text encoder bundle, performs SHA-256 verification with atomic `.part` replacement, and transitions straight into motion generation.
- **Complete Text Encoder Bundle (35 files)**: Fully resolves all 35 required GGML bundle weights (`tokenizer.gguf`, `embedding.gguf`, `final-norm.gguf`, `layer-00.gguf` to `layer-31.gguf`) required by the upstream kimodo.cpp neural text encoder runtime.
- **Hardware GPU & Tensor Runtime Validation**: Real hardware validation dynamically queries Vulkan 1.0 physical devices (discrete GPU / integrated GPU) without requiring a separate Vulkan SDK, checking native GGML DLLs (`ggml.dll`, `ggml-base.dll`, `ggml-cpu.dll`, `ggml-vulkan.dll`).
- **Deterministic Setup State Machine**: Fully deterministic transitions (`Checking`, `NeedsLogin`, `Downloading`, `Verifying`, `Ready`, `Error`). The Generate page is strictly gated against incomplete setups.
- **Strict Offline Mode**: Safe offline operation requires locally verified weights and refuses to falsely enter Ready when required assets are missing.
- **Zero Python / CLI Dependencies**: Packaged distribution requires no Python, pip, huggingface-cli, manual directory creation, or manual file copying.

---

## Installation & First Launch

1. Extract the release ZIP to your chosen directory.
2. Double-click `kimodo_studio.exe`.
3. First-run Setup Wizard launches automatically:
   - Enter your Hugging Face User Access Token (Read scope) for gated model repository access.
   - Click **Download Required Components** to fetch and verify the motion model and text encoder bundle.
   - Wait for SHA-256 verification and tensor runtime checks to complete.
4. Click **Start Creating Motion** to start generating humanoid motions immediately.

*(Note: The Models page is an advanced interface for manual model inspection, verification, and local custom GGUF imports.)*

---

## System Requirements

- **OS**: Windows 10 (Build 19041+) or Windows 11 (64-bit)
- **GPU**: Vulkan 1.0+ capable GPU (NVIDIA RTX 3060/4060 or equivalent recommended)
- **RAM**: 16 GB system RAM recommended for 32-layer LLM text encoder operations
- **Storage**: ~20 GB free disk space for AI model weights
- **Runtime**: Microsoft Visual C++ 2015-2022 Redistributable (x64)

---

## Asset & Model Licenses & Attribution

- **SOMA RP v1.1 Motion Model**: Licensed under the [NVIDIA Open Model License Agreement](https://developer.download.nvidia.com/licenses/nvidia-open-model-license-agreement-june-2024.pdf). Access requires gated authorization on Hugging Face (`kimodo/soma-rp-v1.1`). Not royalty-free redistribution.
- **LLM2Vec Text Encoder Bundle (35 GGUF files)**: Licensed under the [Meta Llama 3 Community License Agreement](https://llama.meta.com/llama3/license/). Access requires gated authorization on Hugging Face (`LocalAI-io/Llama-3-Kimodo-GGML`). Weights are downloaded on-demand after user authentication and are never bundled into the installer.
- **Cesium Man**: `assets/characters/CesiumMan.glb` from Khronos glTF Sample Assets. Copyright © Cesium GS, Inc. Licensed under [Creative Commons Attribution 4.0 International (CC-BY 4.0)](https://creativecommons.org/licenses/by/4.0/).
- **Font Awesome Free**: `fonts/fa-solid-900.ttf` by Fonticons, Inc. (CC BY 4.0 / SIL OFL 1.1).
- **Roboto**: `fonts/Roboto-Regular.ttf` by Christian Robertson (Apache 2.0).
