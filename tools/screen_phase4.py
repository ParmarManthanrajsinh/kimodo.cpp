"""Screen bare-member rename candidates: find receivers of .tok / ->tok and
classify whether any receiver looks like a raylib/cgltf/external object."""
import re
import pathlib
import collections

SRC = pathlib.Path(__file__).resolve().parent.parent / "kimodo-studio" / "src"
STRIP = re.compile(r'"(?:[^"\\]|\\.)*"|//.*|/\*.*?\*/', re.S)

CANDS = [
    "id", "kind", "mode", "text", "fps", "errors", "warnings", "mapping",
    "parents", "parent", "offsets", "scale", "skeleton", "author", "license",
    "installed", "prompt", "jointNames", "vertices", "indices", "position",
    "w", "frameCount", "frameTime", "boneCount", "jointCount", "vertexCount",
    "indexCount", "triangleCount", "valid", "createdAt", "expiresAt", "path",
    "dir", "player", "version", "sha256", "seed", "steps", "progress",
    "status", "theme", "thumbnailPath", "restPosition", "restRotation",
    "restScale", "boneIndices", "boneWeights", "baseColor", "normal",
    "texcoord", "gpuName", "vulkanAvailable", "defaultMap", "diffuseTexture",
]

# Receivers that belong to external (raylib/cgltf/std) types -> must NOT rename
# member tokens accessed on them.
EXTERNAL_RECEIVERS = {
    "node", "skin", "mesh", "prim", "primitive", "data", "gltf", "cgltf",
    "v", "q", "m", "mat", "cam", "camera", "pos", "vec", "t", "w", "h",
    "r", "s", "b", "a", "i", "j", "k", "n", "p", "x", "y", "z", "d", "it",
    "vert", "acc", "attr", "img", "tex", "mat4", "quat", "val", "buf",
}

recv = collections.defaultdict(collections.Counter)
files_of = collections.defaultdict(set)

for p in SRC.rglob("*"):
    if p.suffix not in (".h", ".cpp"):
        continue
    t = STRIP.sub(lambda m: " " * len(m.group(0)),
                  p.read_text(encoding="utf-8", errors="ignore"))
    pat = re.compile(
        r"(\w+(?:\[[^\]\n]{1,24}\])?)(?:\.|->)\b(" + "|".join(CANDS) + r")\b")
    for m in pat.finditer(t):
        recv[m.group(2)][m.group(1)] += 1
        files_of[m.group(2)].add(p.name)

print("token              hits  external_hits  distinct_receivers")
for tok in sorted(recv):
    hits = sum(recv[tok].values())
    ext = {r: c for r, c in recv[tok].items() if r in EXTERNAL_RECEIVERS}
    flag = "  <-- EXTERNAL COLLISION" if ext else ""
    print("%-18s %4d  %4d  %2d%s" % (
        tok, hits, sum(ext.values()), len(recv[tok]), flag))
    if ext:
        print("     external receivers:", sorted(ext.items())[:8])

print("\nTokens NOT seen as member-access (declared but unused?):")
seen = set(recv)
for tok in CANDS:
    if tok not in seen:
        print("  ", tok)
