"""Phase 4: UE-style renames for members and functions (engine v2).

Map JSON format (ordered object; entries applied in order):
  "PROTECT: node->name" : "PROTO_node_name"   regex protection -> sentinel
  "PROTO_node_name"     : "node->name"        restore (last entries)
  "entries_"            : "Entries"           plain word-boundary rename
  {"pattern": "entries\\s*\\(", "replacement": "GetEntries("}  regex entry

All edits operate on code regions only (strings/char literals/comments are
masked). Include lines can be protected via regex entries.

Usage: python rename_phase4.py members <map.json> [--dry]
"""
import json
import re
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent / "kimodo-studio" / "src"

TOKEN_RE = re.compile(r'"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'|//[^\n]*|/\*.*?\*/')
PLACEHOLDER = "\x00"


def strip_noncode(text: str):
    segments = []

    def repl(m):
        segments.append(m.group(0))
        return PLACEHOLDER * len(m.group(0))

    return TOKEN_RE.sub(repl, text), segments


def restore(text: str, segments):
    it = iter(segments)
    return re.sub(PLACEHOLDER + "+", lambda m: next(it), text)


def collect_files():
    return [p for p in ROOT.rglob("*") if p.suffix in (".h", ".cpp", ".c", ".m", ".mm")]


def apply_map(mapping: dict, dry=False):
    """Groups: PROTECT|<regex> -> sentinel first; PROTO_* restore last;
    RE|<regex> and plain keys renamed in between."""
    protect, normal, restore_e = [], {}, {}
    for key, val in mapping.items():
        if key.startswith("PROTECT|"):
            protect.append((key[8:], val))
        elif key.startswith("PROTO_"):
            restore_e[key] = val
        elif key.startswith("RE|"):
            normal["RE|" + key[3:]] = val
        else:
            normal[key] = val
    changed = []
    for f in collect_files():
        raw = f.read_text(encoding="utf-8", errors="ignore")
        stripped, segs = strip_noncode(raw)
        out = stripped
        hits = 0
        for pat_s, repl in protect:
            out, n = re.compile(pat_s).subn(repl, out)
            hits += n
        for key, val in normal.items():
            if key.startswith("RE|"):
                out, n = re.compile(key[3:]).subn(val, out)
            else:
                out, n = re.compile(r"\b" + re.escape(key) + r"\b").subn(val, out)
            hits += n
        for key, val in restore_e.items():
            out, n = re.compile(r"\b" + re.escape(key) + r"\b").subn(val, out)
            hits += n
        if hits == 0:
            continue
        final = restore(out, segs)
        if final != raw:
            if not dry:
                f.write_text(final, encoding="utf-8", newline="\n")
            changed.append((str(f), hits))
    return changed


def cmd_members(mapfile, dry=False):
    mapping = json.loads(pathlib.Path(mapfile).read_text(encoding="utf-8"),
                         object_pairs_hook=lambda pairs: dict(pairs))
    changed = apply_map(mapping, dry=dry)
    total = sum(h for _, h in changed)
    print(("DRY " if dry else "") + "renamed %d occurrences in %d files" % (total, len(changed)))
    for f, h in changed:
        print("  %4d  %s" % (h, f))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    cmd_members(sys.argv[2], dry="--dry" in sys.argv)


if __name__ == "__main__":
    main()
