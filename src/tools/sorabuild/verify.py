"""Compare a sorabuild output tree against the live data (byte for byte) and write the manifest.

  python -E -P verify.py --out <built data dir> --ref <live data dir> [--group fx] [--manifest manifest.json]
  python -E -P verify.py --out <built data dir> --expect manifest.thispc.json      (s168 friend install: no live data,
      every target must match the shipped manifest's size + sha256; prints the build id)
Exit 0 only if every selected target is SAME. The manifest (sha256 + size of every target, build id) is
written only when all targets of all groups are SAME, unless --manifest-anyway.
"""
import argparse
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb  # noqa: E402


def targets(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for ln in f:
            ln = ln.split("#", 1)[0].strip()
            if ln:
                rel, grp = ln.split()
                out.append((rel, grp))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--ref")
    ap.add_argument("--targets", default=os.path.join(sb.HERE, "targets.txt"))
    ap.add_argument("--group", action="append", default=[])
    ap.add_argument("--manifest")
    ap.add_argument("--manifest-anyway", action="store_true")
    ap.add_argument("--expect", help="(s168) a manifest.json to match instead of --ref")
    a = ap.parse_args()
    exp = None
    if a.expect:
        if a.ref or a.manifest:
            raise SystemExit("verify: --expect goes without --ref / --manifest")
        exp = json.load(open(a.expect, encoding="utf-8"))
    tg = targets(a.targets)
    sel = [t for t in tg if not a.group or t[1] in a.group]
    files, bad, counts = {}, 0, {}
    for rel, grp in sel:
        p = os.path.join(a.out, rel)
        if not os.path.exists(p):
            st = "MISSING"
        else:
            size, h = os.path.getsize(p), sb.sha256(p)
            files[rel] = {"size": size, "sha256": h}
            st = "BUILT"
            if exp is not None:
                e = exp["files"].get(rel)
                st = "NOT IN MANIFEST" if not e else ("DIFF size %d vs %d" % (size, e["size"]) if e["size"] != size else
                                                     ("DIFF bytes" if e["sha256"] != h else "SAME"))
            elif a.ref:
                r = os.path.join(a.ref, rel)
                if not os.path.exists(r):
                    st = "NOREF"
                elif os.path.getsize(r) != size:
                    st = "DIFF size %d vs ref %d" % (size, os.path.getsize(r))
                elif sb.sha256(r) != h:
                    st = "DIFF bytes"
                else:
                    st = "SAME"
        ok = st == "SAME" or (not a.ref and exp is None and st == "BUILT")
        bad += not ok
        counts.setdefault(grp, [0, 0])[0 if ok else 1] += 1
        print("%-8s %-44s %s" % (grp, rel, st))
    print("--")
    for g, (o, b) in sorted(counts.items()):
        print("%-8s %3d ok  %3d not" % (g, o, b))
    if exp is not None:
        print("build id %s: %s" % (exp.get("build_id"), "ALL SAME" if not bad and len(sel) == len(tg) else "%d not ok" % bad))
    if a.manifest:
        if not a.ref and not a.manifest_anyway:
            print("manifest NOT written: it needs --ref (a checked build)")
        elif (bad or len(sel) != len(tg)) and not a.manifest_anyway:
            print("manifest NOT written (%d not ok, or not every group selected)" % bad)
        else:
            lines = "".join("%s %d %s\n" % (r, files[r]["size"], files[r]["sha256"]) for r in sorted(files))
            m = {"build_id": hashlib.sha256(lines.encode()).hexdigest()[:16], "files": files}
            with open(a.manifest, "w", encoding="utf-8", newline="\n") as f:
                json.dump(m, f, indent=1, sort_keys=True)
            print("manifest written: %s (build id %s)" % (a.manifest, m["build_id"]))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
