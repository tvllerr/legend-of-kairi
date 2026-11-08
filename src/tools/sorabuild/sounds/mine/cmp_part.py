#!/usr/bin/env python3
"""Compare a mined fragment (or a whole mined recipe) against the old frozen sound recipe (the answer key), dev only.

  python -E -P cmp_part.py <fragment.json> <section...> [--recipe R] [--deep] [--max N | --show N] [--order]

R defaults to the dev reference copy of the old frozen recipe (sounds/dev/recipe_reference.json; the shipped
tools/sorabuild/sounds_recipe.json before it was retired).
Per section (sounds, effects, system, actors, motions, scd_sha256, hud): entries identical / differing / missing (in the
recipe, not in the fragment) / extra (in the fragment only), with the first N diffs (path + key value + ours), and
whether the entry key ORDER is the same (reported always; only required with --order). A list or scalar section
compares as one value.
--deep    for dict-of-dict sections (actors), also compare one level down (per actor: per field) and report those counts.
--order   also report entries whose JSON key order or list order differs although the values are equal (the recipe's
          key order is the pack order of some sfx tools, so it matters for byte-identical outputs).
Exit 0 only when every section is 100% identical (values; with --order also key order).
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def default_recipe():
    for p in (os.path.join(HERE, "..", "dev", "recipe_reference.json"), os.path.join(HERE, "..", "..", "sounds_recipe.json")):
        if os.path.isfile(p):
            return os.path.normpath(p)
    return os.path.normpath(os.path.join(HERE, "..", "dev", "recipe_reference.json"))


def diff(a, b, path, out, limit):
    """first differences between key value a and ours b (dict key order ignored, list order kept)"""
    if len(out) >= limit:
        return
    if isinstance(a, dict) and isinstance(b, dict):
        for k in a:
            if k not in b:
                out.append("%s/%s: missing (key %s)" % (path, k, short(a[k])))
            else:
                diff(a[k], b[k], "%s/%s" % (path, k), out, limit)
            if len(out) >= limit:
                return
        for k in b:
            if k not in a:
                out.append("%s/%s: extra (ours %s)" % (path, k, short(b[k])))
                if len(out) >= limit:
                    return
    elif isinstance(a, list) and isinstance(b, list):
        if len(a) != len(b):
            out.append("%s: list len key %d ours %d (key %s | ours %s)" % (path, len(a), len(b), short(a), short(b)))
            return
        for i, (x, y) in enumerate(zip(a, b)):
            diff(x, y, "%s[%d]" % (path, i), out, limit)
            if len(out) >= limit:
                return
    elif a != b or type(a) != type(b):
        out.append("%s: key %s ours %s" % (path, short(a), short(b)))


def short(v, n=160):
    s = json.dumps(v, separators=(",", ":"))
    return s if len(s) <= n else s[:n] + "..."


def ordered(v):
    """value with dict key order made significant (dict -> list of pairs)"""
    if isinstance(v, dict):
        return [[k, ordered(x)] for k, x in v.items()]
    if isinstance(v, list):
        return [ordered(x) for x in v]
    return v


def cmp_section(name, key, ours, mx, deep, order):
    print("== %s" % name)
    if not isinstance(key, dict) or not isinstance(ours, dict):
        same = key == ours
        print("  %s" % ("identical" if same else "DIFFERS"))
        if not same:
            out = []
            diff(key, ours, name, out, mx)
            for d in out:
                print("   ", d)
        return same and (not order or ordered(key) == ordered(ours))
    ident = [k for k in key if k in ours and key[k] == ours[k]]
    differ = [k for k in key if k in ours and key[k] != ours[k]]
    missing = [k for k in key if k not in ours]
    extra = [k for k in ours if k not in key]
    print("  %d/%d identical, %d differ, %d missing, %d extra" % (len(ident), len(key), len(differ), len(missing), len(extra)))
    if missing:
        print("  missing:", " ".join(missing[:30]))
    if extra:
        print("  extra:", " ".join(extra[:30]))
    ok = not (differ or missing or extra)
    same_order = [k for k in key if k in ours] == [k for k in ours if k in key]
    print("  key order %s%s" % ("same" if same_order else "DIFFERENT", "  -> 100% IDENTICAL" if ok else ""))
    if order:
        if not same_order:
            print("  ORDER: entry key order differs (key %s | ours %s)" % (short(list(key), 120), short(list(ours), 120)))
            ok = False
        bad = [k for k in ident if ordered(key[k]) != ordered(ours[k])]
        if bad:
            print("  ORDER: %d equal entries differ in key/list order: %s" % (len(bad), " ".join(bad[:20])))
            ok = False
    for k in differ:
        if deep and isinstance(key[k], dict) and isinstance(ours[k], dict):
            f_id = [f for f in key[k] if f in ours[k] and key[k][f] == ours[k][f]]
            f_bad = [f for f in key[k] if f not in f_id]
            f_ex = [f for f in ours[k] if f not in key[k]]
            print("  [%s] fields identical %d/%d; differ: %s%s" % (k, len(f_id), len(key[k]), " ".join(f_bad),
                                                                    (" extra: " + " ".join(f_ex)) if f_ex else ""))
            for f in f_bad:
                v = key[k][f]
                if isinstance(v, dict) and isinstance(ours[k].get(f), dict):
                    o = ours[k][f]
                    n_id = sum(1 for x in v if x in o and v[x] == o[x])
                    print("     %s: %d/%d identical, %d differ, %d missing, %d extra" % (
                        f, n_id, len(v), sum(1 for x in v if x in o and v[x] != o[x]), sum(1 for x in v if x not in o),
                        sum(1 for x in o if x not in v)))
        out = []
        diff(key[k], ours[k], "%s/%s" % (name, k), out, mx)
        for d in out:
            print("    ", d)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fragment")
    ap.add_argument("sections", nargs="+")
    ap.add_argument("--recipe", default=default_recipe())
    ap.add_argument("--max", "--show", dest="max", type=int, default=6)
    ap.add_argument("--deep", action="store_true")
    ap.add_argument("--order", action="store_true")
    a = ap.parse_args()
    key = json.load(open(a.recipe, encoding="utf-8"))
    ours = json.load(open(a.fragment, encoding="utf-8"))
    ok = True
    for s in a.sections:
        if s not in key:
            print("== %s: not a recipe section" % s)
            ok = False
            continue
        if s not in ours:
            print("== %s: MISSING in the fragment" % s)
            ok = False
            continue
        ok = cmp_section(s, key[s], ours[s], a.max, a.deep, a.order) and ok
    print("RESULT:", "ALL IDENTICAL" if ok else "DIFFERENCES")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
