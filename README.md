# kimodo.cpp

GGML/C++ implementation of NVIDIA's Kimodo text-to-motion model.

## Status

The five released Kimodo motion checkpoints accept either a UTF-8 prompt or a
precomputed LLM2Vec embedding and generate local rotations plus root
translations on CPU or Vulkan:

- SMPL-X RP v1: 22 joints
- SOMA RP/SEED v1.1: the predicted compact 30-joint control skeleton
- G1 RP/SEED v1: 34 Unitree G1 joints

NVIDIA's Python API expands SOMA's predicted 30 joints to a relaxed-hand
77-joint presentation skeleton. The native API currently returns the 30 joints
the model actually predicts. The text encoder uses eight-layer Vulkan chunks by
default; set `KIMODO_TEXT_LAYER_CHUNK=1..32` to tune VRAM use.

Included: checked GGUF loading, safetensors conversion, DDIM sampling, C/C++
APIs, conditioned multi-prompt transitions, CPU/Vulkan parity tests,
skeleton-only GLB export, and a local text-to-motion demo. General constraint
input, 77-joint SOMA expansion, skinned-mesh GLB export, and quantised models
are not implemented yet.

## Build and test on Linux

Install a C++23 compiler, CMake 3.25+, Ninja, Python 3 with the Hugging Face
CLI (`pip install huggingface_hub`), and the Vulkan loader/headers for Vulkan
support. GGML is a pinned Git submodule:

```sh
git submodule update --init --recursive
scripts/download_gguf_weights.sh --output "$PWD" --model soma-rp-v1.1
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The standard test suite requires the local motion GGUF, text bundle, and
fixtures. It never downloads weights by itself. `release`, `asan-ubsan`, and
`fuzz` presets are also available.

Nix is optional and provides these dependencies reproducibly:

```sh
nix develop path:. --command cmake --preset debug
nix develop path:. --command cmake --build --preset debug
```

For sanitizer work:

```sh
nix develop path:. --command cmake --preset asan-ubsan
nix develop path:. --command cmake --build --preset asan-ubsan
nix develop path:. --command env \
  LD_LIBRARY_PATH="$PWD/build/asan-ubsan/ggml/src:$PWD/build/asan-ubsan/ggml/src/ggml-vulkan:$LD_LIBRARY_PATH" \
  ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --preset asan-ubsan --output-on-failure
```

Leak detection is disabled because Vulkan loader/driver allocations are global
to the process. The GGUF parser fuzzer requires Clang.

## Build on Windows

Prerequisites: Git, Python 3.10+ with `huggingface_hub`, Visual Studio 2022
with C++ tools, CMake 3.25+, and (for GPU inference) the Vulkan SDK with
`VULKAN_SDK` set and `Bin\glslc.exe` present. Go is only needed for the
browser demo. `install_kimodo_cpp.bat` installs all of this via winget, or
install each manually.

GGML is a pinned Git submodule:

```powershell
cd E:\kimodo.cpp
git submodule update --init --recursive
```

Configure with the VS 2022 generator (Vulkan ON for GPU, OFF for CPU-only):

```powershell
# GPU
cmake -DKIMODO_BUILD_TESTS=OFF -DKIMODO_ENABLE_VULKAN=ON -DGGML_VULKAN=ON `
  -B build -G "Visual Studio 17 2022" -A x64
# CPU-only (no Vulkan SDK needed)
cmake -DKIMODO_BUILD_TESTS=OFF -DKIMODO_ENABLE_VULKAN=OFF -DGGML_VULKAN=OFF `
  -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

This produces `build\Release\kmd-generate.exe` (plus `kmd-inspect.exe`,
`kmd-encode.exe`) with runtime DLLs in `build\bin\Release\`. Both directories
must be on `PATH` when running the exe directly; `generate.ps1` and
`demo.ps1` already do this.

Download weights (motion model is ungated, text bundle is gated):

```powershell
# motion weights only (~1.1 GB, no login needed)
python scripts/download_gguf_weights.py --model soma-rp-v1.1 --output . --motion-only
# full bundle incl. Llama-3 text encoder (~8 GB, login + licence needed)
hf auth login
python scripts/download_gguf_weights.py --model soma-rp-v1.1 --output .
```

The gated text bundle requires accepting
[LocalAI-io/Llama-3-Kimodo-GGML](https://huggingface.co/LocalAI-io/Llama-3-Kimodo-GGML)
with a free Hugging Face account before `hf auth login`. After the full
download, `generated\llm2vec-text-bundle\` must contain 35 `.gguf` files
(`tokenizer.gguf`, `embedding.gguf`, `final-norm.gguf`,
`layer-00.gguf` … `layer-31.gguf`).

Generate and demo:

```powershell
.\generate.ps1 "a person walking forward enthusiastically and waving their right hand"
.\demo.bat   # http://localhost:8094
```

Troubleshooting: `sequence: exit status 1: text model must be a component
directory` means `generated\llm2vec-text-bundle` is missing or incomplete
(`src/llm_text_encoder.cpp` requires that component directory). Complete the
gated text-bundle download above and restart the demo.

## API

`include/kimodo/kimodo_capi.h` is the C API. Model loading checks the motion
GGUF and text bundle before inference. Use `kimodo_generate_embedding` for
4096 F32 values or `kimodo_generate` for text. Both return the selected model's
root translations and local XYZW rotations; query the joint count from the
result rather than assuming a fixed skeleton.

## Demo

After building the debug preset and downloading the native GGUF bundle:

```sh
go run ./demo -addr 0.0.0.0:8094
```

Open `http://localhost:8094`. The left sidebar contains the prompt and a
persistent history; choosing a previous animation restores its prompt for a
new generation. Every successful animation also writes a standalone
`animation.glb` beside its raw streams, for example
`demo-output/<animation-id>/animation.glb`. It contains the selected animated
node hierarchy (no mesh), ready to copy into a Three.js project. It is also
available from `/api/animations/<animation-id>/animation.glb` while the demo
is running.

## Weights

Ready-to-run native GGML weights are published under the Hugging Face
`LocalAI-io` organisation (not GitHub's `localai-org`). The reusable
[Llama-3-Kimodo-GGML](https://huggingface.co/LocalAI-io/Llama-3-Kimodo-GGML)
text encoder is separate from the four redistributable motion repositories,
each of which preserves a one-to-one relationship to its NVIDIA upstream:

- [Kimodo-SOMA-RP-v1.1-GGML](https://huggingface.co/LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML)
- [Kimodo-SOMA-SEED-v1.1-GGML](https://huggingface.co/LocalAI-io/Kimodo-SOMA-SEED-v1.1-GGML)
- [Kimodo-G1-RP-v1-GGML](https://huggingface.co/LocalAI-io/Kimodo-G1-RP-v1-GGML)
- [Kimodo-G1-SEED-v1-GGML](https://huggingface.co/LocalAI-io/Kimodo-G1-SEED-v1-GGML)

Download one or repeat `--model` to install several:

```sh
scripts/download_gguf_weights.sh --output "$PWD" \
  --model soma-rp-v1.1 --model g1-rp-v1
```

The installer verifies each published manifest and SHA-256 hashes. Use
`--motion-only` when supplying a precomputed 4096-float LLM2Vec embedding.
SMPL-X RP is deliberately absent from the published-weight installer: its
internal-R&D licence prohibits distributing derivative models, so it must be
converted locally after the user obtains the upstream checkpoint under its
gated terms.

The text bundle includes converted Meta Llama 3 material and retains its
separate terms. Review every selected model card before downloading or
redistributing.

## License

The C++ port and its original tooling are licensed under Apache-2.0; see
[LICENSE](LICENSE). GGML and the model weights retain their respective
licences.

| Motion checkpoint | Upstream terms | Commercial use |
| --- | --- | --- |
| Kimodo-SMPLX-RP-v1 | [NVIDIA Internal Scientific Research and Development Model License](https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-internal-scientific-research-and-development-model-license/) | No; internal, non-production R&D only; derivative model redistribution is prohibited |
| SOMA RP/SEED v1.1 | [NVIDIA Open Model License](https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-open-model-license/) | Permitted by the model licence |
| G1 RP/SEED v1 | [NVIDIA Open Model License](https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-open-model-license/) | Permitted by the model licence |

The SMPL-X warning is about NVIDIA's trained Kimodo checkpoint, not the mere
fact that its output uses an SMPL-X-shaped 22-joint hierarchy. Converting that
checkpoint to GGUF is a new runtime representation of the same weights and does
not replace its licence. Skeleton names, parent links, and the Apache-2.0 port
source do not by themselves make the SOMA or G1 checkpoints non-commercial.
The SMPL-X Hugging Face metadata, model card, and access terms identify the
internal-R&D licence; treat those restrictive terms as controlling even though
an apparently inconsistent `LICENSE` file has also appeared in that upstream
repository.

### Regenerating the bundle

This is only needed to reproduce a conversion. The SMPL-X checkpoint and Llama
base model are gated. After accepting their Hugging Face licences and
authenticating, download the exact revisions and hash manifests with:

```sh
nix develop path:. --command hf auth login
scripts/download_weights.sh --output "$PWD/models" --with-text \
  --model smplx-rp-v1 --model soma-rp-v1.1 --model soma-seed-v1.1 \
  --model g1-rp-v1 --model g1-seed-v1
```

Convert the local LLM2Vec model to the native component bundle with:

```sh
nix develop path:. --command scripts/convert_llm2vec_bundle.sh \
  "$PWD/models/llama3-8b-instruct-base" "$PWD/generated/llm2vec-text-bundle"
```

Validate a prospective release without network access, then explicitly upload
it from an account allowed to publish to `LocalAI-io`:

```sh
nix develop path:. --command python scripts/publish_gguf.py --component motion \
  --motion-model soma-rp-v1.1
nix develop path:. --command python scripts/publish_gguf.py --component motion \
  --motion-model soma-rp-v1.1 --upload --confirm-upstream-licences
nix develop path:. --command python scripts/publish_gguf.py --component text \
  --upload --confirm-upstream-licences
```
