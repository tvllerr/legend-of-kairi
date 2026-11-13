"""(s223) meltycheck: nothing under Melty's licence in a public folder (the GitHub release zip, the public source snapshot).

  python tools/installer/meltycheck.py <folder> [<folder> ...]      -> exit 1 and the list when something is found

Melty's connector (runtime/melty/*: under Melty's own licence, not MIT) and the Melty setup (melty-setup/*) ship only in
the Melty package. Looked for:
  - their folders and file names anywhere in the tree;
  - their licence lines and the connector's own code words in any file (text or binary);
  - the strings only the connector's code puts into sora_mod.dll (a DLL built without NO_MELTY=1 has them).
The marker words are put together at run time, so this file itself never matches (it is dev-only: mkzip SKIP_TOOLS).
"""
import os
import sys

MELTY_DIRS = ("runtime/melty", "melty-setup")
MELTY_NAMES = {"sora_melty.cpp", "sora_melty.h", "melty_net.h", "melty_net_impl.cpp", "melty_addon.h", "melty_agent_input.h",
               "melty_addon_test.cpp", "melty_cap_test.cpp", "melty.properties"}
_M = "Mel" + "ty"
# licence lines of Melty's files (runtime/melty/melty_addon.h, README.txt) and the connector's own code words
TEXT_MARKERS = [s.encode() for s in ("LicenseRef-" + _M + "-Mashups-Only", "For " + _M + " mashups only",
                                     "licensed for " + _M + " mashups", _M.lower() + "::net", _M.upper() + "_NET_IMPLEMENTATION")]
# (s223 step 5) strings only runtime/melty/*.cpp put into sora_mod.dll: a DLL built with them has the connector in it.
# Found by diffing `strings` of the DLL built with and without NO_MELTY=1 (sb/s223/github/dll/melty_only_strings.txt).
#   N5<m>3net     the connector's C++ namespace in its type names (9087 strings of the s223 Melty build, 0 without)
#   .net.<m>.gg/  Melty Net's server address;  <M>_NET_UDP_BIND / SAVAGEWIND_SORA_<M>_DIRECT: the connector's settings
# (none of them is in our own MIT files: test_meltycheck scans tools/; the s223 run scanned runtime/ minus runtime/melty)
BIN_MARKERS = [s.encode() for s in (
    "N5" + _M.lower() + "3net", ".net." + _M.lower() + ".gg/", _M.upper() + "_NET_UDP_BIND", "SAVAGEWIND_SORA_" + _M.upper() + "_DIRECT",
)]
SKIP_DIRS = {".git"}


def scan(root, max_bytes=1 << 31):
    """-> the problems found under root (relative paths + what)"""
    root = os.path.abspath(root)
    bad = []
    markers = TEXT_MARKERS + BIN_MARKERS
    walk = [(os.path.dirname(root), [], [os.path.basename(root)])] if os.path.isfile(root) else os.walk(root)
    if os.path.isfile(root):
        root = os.path.dirname(root)
    elif not os.path.isdir(root):
        return [root + "  (not found)"]
    for d, dirs, fs in walk:
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        rel_d = os.path.relpath(d, root).replace("\\", "/")
        for x in dirs:
            r = (x if rel_d == "." else rel_d + "/" + x)
            if any(r == m or r.endswith("/" + m) for m in MELTY_DIRS):
                bad.append(r + "/  (Melty's folder)")
        for n in fs:
            r = n if rel_d == "." else rel_d + "/" + n
            if n.lower() in MELTY_NAMES:
                bad.append(r + "  (Melty's file)")
                continue
            p = os.path.join(d, n)
            try:
                if os.path.getsize(p) > max_bytes:
                    continue
                with open(p, "rb") as f:
                    b = f.read()
            except OSError as e:
                bad.append(r + "  (could not read: %s)" % e)
                continue
            for m in markers:
                if m in b:
                    bad.append(r + "  (has %r)" % m.decode())
                    break
    return bad


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    bad = []
    for a in argv:
        bad += [os.path.join(a, x) for x in scan(a)]
    if bad:
        print("meltycheck: Melty's content found (it must not be public):")
        for b in bad[:50]:
            print("  " + b)
        return 1
    print("meltycheck: %s: nothing of Melty's" % ", ".join(argv))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
