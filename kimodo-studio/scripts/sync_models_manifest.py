"""Sync-check kimodo-studio/config/models.json against authoritative HF MANIFEST.json files.

Compares exact path/size/sha256 for every required asset. Never invents hashes:
any drift fails with a non-zero exit so CI blocks a stale registry.

Usage:
    python scripts/sync_models_manifest.py [--check-only]
    python scripts/sync_models_manifest.py --manifest-dir <dir>  # offline check against local copies

Requires: Python 3.9+, requests (only for live fetch; --manifest-dir skips network).
"""
import argparse
import json
import sys
from pathlib import Path

STUDIO_DIR = Path(__file__).resolve().parents[1]
MODELS_JSON = STUDIO_DIR / "config" / "models.json"

REPOS = {
    "LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML": "motion",
    "LocalAI-io/Llama-3-Kimodo-GGML": "text",
}


def manifest_url(repo: str) -> str:
    return f"https://huggingface.co/{repo}/raw/main/MANIFEST.json"


def fetch_manifest(repo: str) -> dict:
    import urllib.request

    with urllib.request.urlopen(manifest_url(repo), timeout=30) as resp:
        if resp.status != 200:
            raise RuntimeError(f"HTTP {resp.status} fetching {manifest_url(repo)}")
        return json.loads(resp.read().decode("utf-8"))


def load_models_json() -> dict:
    with open(MODELS_JSON, "r", encoding="utf-8") as f:
        return json.load(f)


def check_repo(repo: str, manifest: dict, registry: dict) -> list:
    errors = []
    manifest_files = {f["path"]: f for f in manifest.get("files", [])}
    entries = {e["id"]: e for e in registry.get("models", [])}

    if repo == "LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML":
        entry = entries.get("soma-rp-v1.1")
        if entry is None:
            return [f"missing registry entry id=soma-rp-v1.1 for {repo}"]
        for key in ("repo", "remotePath", "sizeBytes", "sha256"):
            if key not in entry:
                errors.append(f"soma-rp-v1.1 missing key {key}")
        mf = manifest_files.get(entry.get("remotePath", ""))
        if mf is None:
            errors.append(
                f"soma-rp-v1.1 remotePath {entry.get('remotePath')} not in MANIFEST"
            )
        else:
            if entry.get("sizeBytes") != mf.get("bytes"):
                errors.append(
                    f"soma-rp-v1.1 size drift: registry={entry.get('sizeBytes')} manifest={mf.get('bytes')}"
                )
            if (entry.get("sha256") or "").lower() != (mf.get("sha256") or "").lower():
                errors.append("soma-rp-v1.1 sha256 drift vs MANIFEST")
        if entry.get("repo") != repo:
            errors.append(f"soma-rp-v1.1 repo drift: {entry.get('repo')} != {repo}")
        return errors

    if repo == "LocalAI-io/Llama-3-Kimodo-GGML":
        entry = entries.get("llm2vec-text-bundle")
        if entry is None:
            return [f"missing registry entry id=llm2vec-text-bundle for {repo}"]
        if entry.get("repo") != repo:
            errors.append(f"llm2vec-text-bundle repo drift: {entry.get('repo')} != {repo}")
        reg_files = {
            f.get("remotePath"): f for f in entry.get("files", [])
        }
        if len(reg_files) != len(manifest_files):
            errors.append(
                f"llm2vec-text-bundle file count drift: registry={len(reg_files)} manifest={len(manifest_files)}"
            )
        for path, mf in manifest_files.items():
            rf = reg_files.get(path)
            if rf is None:
                errors.append(f"text bundle missing registry file for {path}")
                continue
            if rf.get("sizeBytes") != mf.get("bytes"):
                errors.append(
                    f"text bundle size drift {path}: registry={rf.get('sizeBytes')} manifest={mf.get('bytes')}"
                )
            if (rf.get("sha256") or "").lower() != (mf.get("sha256") or "").lower():
                errors.append(f"text bundle sha256 drift {path}")
        # The legacy backend requires the full 35-file directory bundle.
        for required in (
            "generated/llm2vec-text-bundle/tokenizer.gguf",
            "generated/llm2vec-text-bundle/embedding.gguf",
            "generated/llm2vec-text-bundle/final-norm.gguf",
        ):
            if required not in reg_files:
                errors.append(f"text bundle missing required component {required}")
        return errors

    return [f"unknown repo {repo}"]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest-dir", default=None,
                        help="offline directory holding <repo-slug>/MANIFEST.json copies")
    args = parser.parse_args()

    registry = load_models_json()
    failures: list = []
    for repo in REPOS:
        try:
            if args.manifest_dir:
                slug = repo.replace("/", "__")
                with open(Path(args.manifest_dir) / slug / "MANIFEST.json", encoding="utf-8") as f:
                    manifest = json.load(f)
            else:
                manifest = fetch_manifest(repo)
        except Exception as exc:  # noqa: BLE001 - report, don't crash
            failures.append(f"{repo}: failed to load MANIFEST: {exc}")
            continue
        failures.extend(check_repo(repo, manifest, registry))

    if failures:
        print("models.json MANIFEST sync FAILED:")
        for line in failures:
            print(f"  - {line}")
        return 1
    print("models.json matches authoritative MANIFEST.json for all required repos.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
