"""s160: check our HD texture answers against the PHH oracle's stored textures (bytes).
  python -E -P texcheck.py <kh2_fx.bin> <out dir> <case>...    (needs <case>_A.txt + <case>.cmp.txt from cmpone.sh)
Our TEXID lines name the asset (texcommon / fx file + "-N.dds"); the compare's map gives the oracle texture index.
The asset bytes come from D:/KH2Extract/<pkg>/remastered/<file>/<asset> (pkg from the run's HD table line)."""
import ast, os, re, struct, sys

X = "D:/KH2Extract"


def oracle_tex(path):
    f = open(path, "rb")
    h = f.read(24)
    assert h[:4] == b"PHF2", h[:4]
    _, ntex, _, ttab, _ = struct.unpack_from("<5I", h, 4)
    f.seek(ttab)
    recs = [struct.unpack("<II", f.read(8)) for _ in range(ntex)]
    return f, recs


def main():
    fx, out, cases = sys.argv[1], sys.argv[2], sys.argv[3:]
    f, recs = oracle_tex(fx)
    bad = 0
    for c in cases:
        run = open(os.path.join(out, c + "_A.txt"), encoding="utf-8", errors="replace").read()
        cmpt = open(os.path.join(out, c + ".cmp.txt"), encoding="utf-8", errors="replace").read()
        m = re.search(r"texture map ours->oracle: (\{.*?\})", cmpt)
        tmap = ast.literal_eval(m.group(1)) if m else {}
        src = {}
        for tag, tab in (("texcommon", c + ".texhd"), ("fx", c + ".fxhd")):
            pkg = open(os.path.join(out, tab)).readline().split()[1]
            src[tag] = pkg
        for line in re.findall(r"^TEXID .*$", run, re.M):
            p = line.split()
            tid, kind = int(p[1]), p[2]
            if kind != "hd":
                print(f"{c}: TEXID {tid} is {kind} (not HD)"); bad = 1; continue
            tag, asset = p[3], p[4]
            rel = "effect/texcommon.dpd" if tag == "texcommon" else open(os.path.join(out, c + ".fxname")).read().strip()
            ours = open(f"{X}/{src[tag]}/remastered/{rel}/{asset}", "rb").read()
            oid = tmap.get(tid)
            hits = [i for i in (oid, oid - 1 if oid else None) if i is not None and 0 <= i < len(recs)
                    and recs[i][1] == len(ours) and (f.seek(recs[i][0]) or True) and f.read(recs[i][1]) == ours]
            ok = oid is not None and hits
            print(f"{c}: TEXID {tid} {tag} {asset} -> oracle {oid}: {'SAME bytes (index ' + str(hits[0]) + ')' if ok else 'DIFFERENT'}")
            bad |= not ok
    print("TEXTURES SAME" if not bad else "TEXTURES DIFFER")
    return bad


if __name__ == "__main__":
    sys.exit(main())
