"""kh2list (s168, dev only): freeze the list of KH2 extract files sorabuild reads -> kit/kh2_files.json.

  python -E -P kit/kh2list.py --kh2 D:/KH2Extract --read <atime trace json: [[rel, size], ...]> [--out kit/kh2_files.json]

The trace comes from a clean sorabuild run with last-access times reset first (sb/s168inst/tools/atime_*.py).
Output = our names + sizes + sha256 only (no game bytes): kh2pick extracts exactly these from the player's .hed/.pkg and checks them.
"""
import argparse
import hashlib
import json
import os


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--read", required=True)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "kh2_files.json"))
    a = ap.parse_args()
    rows = []
    rels = {r for r, _ in json.load(open(a.read))}
    rels.add("kh2_first/original/00battle.bin")  # sb.PROBES["kh2"]: only stat()ed (no atime), must exist
    for rel in rels:
        p = os.path.join(a.kh2, rel)
        rows.append([rel, os.path.getsize(p), sha256(p)])
    rows.sort()
    pk = {}
    for r in rows:
        pk[r[0].split("/")[0]] = pk.get(r[0].split("/")[0], 0) + 1
    doc = {"what": "KH2 extract files read by sorabuild (OpenKH IdxImg layout); kh2pick extracts only these",
           "packages": pk, "bytes": sum(r[1] for r in rows), "files": rows}
    with open(a.out, "w", newline="\n") as f:
        f.write('{"what": %s,\n "packages": %s,\n "bytes": %d,\n "files": [\n' % (json.dumps(doc["what"]), json.dumps(pk), doc["bytes"]))
        f.write(",\n".join("  " + json.dumps(r) for r in rows))
        f.write("\n ]}\n")
    print("kh2list: %d files, %.2f GB -> %s" % (len(rows), doc["bytes"] / 1e9, a.out))


if __name__ == "__main__":
    main()
