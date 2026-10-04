# Kimodo Studio Release (KimodoStudio-0.1.0-03421d0-windows-x64-20261005-001658)

## Package Info
- **File**: `KimodoStudio-0.1.0-03421d0-windows-x64-20261005-001658.zip`
- **Size**: 18.31 MB
- **SHA-256**: `AAEBDE24CFD37081EFEDF0C680F9C6F3633085CB33913B4E60ECBF430660DC1C`
- **Platform**: Windows 10/11 x64

## System Requirements
- Windows 10 (Build 19041+) or Windows 11 64-bit
- Vulkan-capable GPU (NVIDIA RTX 3060/4060 or equivalent recommended)
- Microsoft Visual C++ 2015-2022 Redistributable (x64)

## Installation & First Launch
1. Extract `KimodoStudio-0.1.0-03421d0-windows-x64-20261005-001658.zip` to your desired directory.
2. Launch `kimodo_studio.exe`.
3. First-Run Setup Wizard launches automatically:
   - Authenticate with Hugging Face if access to gated model weights is required.
   - Click **Download Required Components** to automatically fetch and verify the SOMA RP motion model and complete 35-file text encoder bundle.
   - Setup validates GGML tensor runtime and Vulkan GPU acceleration.
4. Click **Start Creating Motion** to generate animations.

> *Note: The Models page is an advanced interface for manual model inspection and local imports.*

## Model Licensing & Attribution
- **SOMA RP v1.1 Motion Model**: Licensed under the NVIDIA Open Model License Agreement (Hugging Face `LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML`; access is validated for every required repository at setup time).
- **LLM2Vec Text Encoder Bundle (35 GGUF files)**: Licensed under the Meta Llama 3 Community License Agreement (Hugging Face `LocalAI-io/Llama-3-Kimodo-GGML`; access is validated for every required repository at setup time).
- **CesiumMan Character Asset**: CC-BY 4.0 (Cesium GS, Inc.).
- No restricted model weights are bundled into the distribution archive; all weights are downloaded on-demand after user authentication.
