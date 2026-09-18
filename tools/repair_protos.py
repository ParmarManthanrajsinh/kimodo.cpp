"""Repair leaked PROTO_* sentinels from Phase 4a/4b by restoring arrows
positionally against HEAD.

Strategy:
- Arrow sentinels: each run (PROTO_ARROW_)+ absorbed '<receiver>->' per unit.
  Zip units (in order) against code-region '\\w+->' matches from the HEAD
  version of the same file; validate the member text after each arrow matches.
- Simple sentinels: fixed restore to original pre-rename text.
"""
import re
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent / "kimodo-studio" / "src"
REPO = ROOT.parent.parent

TOKEN_RE = re.compile(r'"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'|//[^*\n]*|/\*.*?\*/')
PLACEHOLDER = "\x00"
RUN_RE = re.compile(r'((?:PROTO_ARROW_)+)(\w*)')

SIMPLE = {
    "PROTO_std_time": "std::time(",
    "PROTO_std_move": "std::move",
    "PROTO_std_remove": "std::remove",
    "PROTO_CAM_POS": "camera_.position",
    "PROTO_VERT_POS": "vert.position",
    "PROTO_VI_POS": "vertices[i].position",
    "PROTO_V_POS": "v.position",
    "PROTO_DOT_RESET": ".reset()",
}


def strip_noncode(text):
    segs = []

    def repl(m):
        segs.append(m.group(0))
        return PLACEHOLDER * len(m.group(0))

    return TOKEN_RE.sub(repl, text), segs


def restore(text, segs):
    it = iter(segs)
    return re.sub(PLACEHOLDER + "+", lambda m: next(it), text)


def head_text(rel):
    r = subprocess.run(
        ["git", "show", "HEAD:kimodo-studio/src/" + rel.as_posix()],
        capture_output=True, check=True, cwd=REPO,
    )
    return r.stdout.decode("utf-8", errors="replace")


def repair_file(path):
    rel = path.relative_to(ROOT)
    raw = path.read_text(encoding="utf-8", errors="ignore")
    if "PROTO_" not in raw:
        return None
    stripped, segs = strip_noncode(raw)

    for key, val in SIMPLE.items():
        stripped = stripped.replace(key, val)

    ht, _ = strip_noncode(head_text(rel))
    receivers = [m.group(1) for m in re.finditer(r"(\w+)->", ht)]
    head_members = []
    for m in re.finditer(r"(\w+)->", ht):
        tail = re.match(r"\s*(\w+)", ht[m.end():])
        head_members.append(tail.group(1) if tail else "<none>")

    runs = list(RUN_RE.finditer(stripped))
    total_units = sum(m.group(1).count("PROTO_ARROW_") for m in runs)
    if total_units != len(receivers):
        return "MISMATCH %s: units=%d head_arrows=%d" % (rel, total_units, len(receivers))

    it = iter(zip(receivers, head_members))
    idx = [0]

    def repl(m):
        units = m.group(1).count("PROTO_ARROW_")
        tail = m.group(2)
        cur_mem = re.match(r"\s*(\w+)", tail).group(1) if tail else None
        out = []
        for k in range(units):
            recv, hmem = next(it)
            # Intermediate units' members were absorbed as the next receiver;
            # only the last unit's member survives as the tail.
            if k == units - 1 and cur_mem and cur_mem != hmem:
                raise ValueError("%s: member mismatch at arrow %d: head=%s cur=%s"
                                 % (rel, idx[0], hmem, cur_mem))
            idx[0] += 1
            out.append(recv + "->")
        return "".join(out) + tail

    try:
        fixed = RUN_RE.sub(repl, stripped)
    except ValueError as e:
        return str(e)

    final = restore(fixed, segs)
    if final != raw:
        path.write_text(final, encoding="utf-8", newline="\n")
    return "OK %s: %d arrows restored" % (rel, total_units)


def main():
    files = [p for p in ROOT.rglob("*") if p.suffix in (".h", ".cpp", ".c", ".m")]
    results = [r for f in files if (r := repair_file(f))]
    for r in results:
        print(r)
    leftovers = subprocess.run(
        ["grep", "-rl", "PROTO_", str(ROOT)], capture_output=True, text=True)
    if leftovers.stdout.strip():
        print("LEFTOVER SENTINELS:")
        print(leftovers.stdout)
        sys.exit(1)
    print("All sentinels repaired.")


if __name__ == "__main__":
    main()
