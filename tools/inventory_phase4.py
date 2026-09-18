"""Inventory Phase 4 rename candidates from headers (members + methods)."""
import re
import pathlib
import collections

SRC = pathlib.Path(__file__).resolve().parent.parent / "kimodo-studio" / "src"

STRIP = re.compile(r'"(?:[^"\\]|\\.)*"|//.*|/\*.*?\*/', re.S)
KEYWORDS = {
    "if", "for", "while", "switch", "sizeof", "return", "catch", "operator",
    "decltype", "static_cast", "reinterpret_cast", "const_cast", "dynamic_cast",
    "explicit", "noexcept", "template", "defined", "emit",
}
EXTERNAL = {
    "name", "count", "buffer", "nodes", "joints", "data", "type", "size",
    "min", "max", "uri", "values", "studio", "screen",
}

und = collections.Counter()
bare = collections.Counter()
fn = collections.Counter()

for p in SRC.rglob("*.h"):
    text = STRIP.sub(lambda m: " " * len(m.group(0)),
                     p.read_text(encoding="utf-8", errors="ignore"))
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        if "(" in s and re.search(r"\)\s*(const)?\s*[;{]?\s*$", s):
            m = re.search(r"\b([a-z][a-zA-Z0-9]*)\s*\(", s)
            if m and m.group(1) not in KEYWORDS and not re.match(
                    r"^(if|for|while|switch|sizeof|catch)\b", s):
                fn[m.group(1)] += 1
            continue
        if "(" in s:
            continue
        m = re.match(r"^[\w:<>,&*\s\[\]\(\)]*?[\s*&]([a-z][a-zA-Z0-9_]*)\s*(?:;|=|\{)", s)
        if not m:
            continue
        tok = m.group(1)
        if tok.endswith("_"):
            und[tok] += 1
        elif tok not in EXTERNAL:
            bare[tok] += 1

print("=== underscore members (%d) ===" % len(und))
for k, c in sorted(und.items()):
    print("%3d  %s_" % (c, k))
print("\n=== bare members (%d) ===" % len(bare))
for k, c in sorted(bare.items()):
    print("%3d  %s" % (c, k))
print("\n=== member functions in headers (%d) ===" % len(fn))
for k, c in sorted(fn.items()):
    print("%3d  %s" % (c, k))
