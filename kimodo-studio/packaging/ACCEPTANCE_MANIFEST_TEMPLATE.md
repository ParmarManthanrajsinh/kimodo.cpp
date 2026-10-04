# Kimodo Studio — Clean-Machine Acceptance Manifest (template)

> Copy this file to `ACCEPTANCE_MANIFEST_<version>_<date>.md` and fill it in on the
> assessor-provisioned VM. It ships next to the portable ZIP and `SHA256SUMS`.

## 1. Package under test

- Package filename:
- Package SHA-256 (from `SHA256SUMS`):
- `kimodo_studio --version` output:
- `KimodoAdapter::AbiVersion()` / `kimodo_abi_version()`:
- Git hash / build date:
- **Distribution status: UNSIGNED portable ZIP — not a signed production release.**
  Expect Windows SmartScreen / Smart App Control warnings. Verify the SHA-256
  before launching.

## 2. Acceptance environment (assessor-provisioned, fresh)

- OS / build (Windows 10/11 x64):
- VM snapshot / machine ID:
- GPU / driver (primary gate: Vulkan-capable NVIDIA GPU):
- CPU-fallback result (secondary only, if applicable):
- `%LOCALAPPDATA%\KimodoStudio\models` confirmed absent before launch: yes/no
- No dev dependencies present (Python / Git / HF CLI / compiler): yes/no

## 3. Hugging Face authentication (runtime only, never shipped)

- Auth flow used (device-code browser / Settings token entry):
- Token shipped, committed, or logged: **never** (assessor supplies at runtime)
- `whoami` username:

## 4. Repository access (every required repo, public or gated)

| Repository | HTTP / access verdict | Notes |
|---|---|---|
| `LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML` | | |
| `LocalAI-io/Llama-3-Kimodo-GGML` | | |

On failure record the exact repo/path + HTTP status (401/403/404). Setup must
NOT be marked Ready; the dependency must NOT be bypassed.

## 5. Downloads + manifest/SHA-256 verification

- Motion `models/kimodo-soma-rp-v1.1-f32.gguf`: expected size 1133166784,
  expected SHA-256 `3bf1229f…948ee4`; actual size / SHA-256 / verdict:
- Text bundle (all 35 files: `tokenizer.gguf`, `embedding.gguf`,
  `final-norm.gguf`, `layer-00.gguf` … `layer-31.gguf`): per-file size +
  SHA-256 vs `config/models.json` / published `MANIFEST.json` (attach table
  or log excerpt):
- `SetupManager` state reached Ready with motion + text + character +
  runtime verified (attach `GetStatusSummary()`):

## 6. Runtime / generation / export

- `RuntimeValidator`: DLL set, `runtime_dlls_valid`, GPU name,
  `gpu_accelerated`, `vulkan_status_message`:
- Generation params (prompt / seed / frames / steps):
- `kimodo_model_load` result (bundle directory, never `tokenizer.gguf`):
- `MotionResult`: frames / joints:
- Viewport playback confirmed: yes/no (screenshot):
- Library save confirmed: yes/no:
- Export artifact (BVH/GLB path + bytes): yes/no:

## 7. Verdict

- [ ] PASS — clean install → auth → download → verify → load → generate →
      viewport → export, all on theordo fresh VM.
- [ ] FAIL — reason / exact failing step / logs attached:

Tester / date:
