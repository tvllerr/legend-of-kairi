#!/bin/bash
# The Linux release package of Legend of Kairi: Legend-of-Kairi-<version>-linux-x86_64.tar.gz (tar keeps the exec bits).
# (s282, release 2) The package builds the mod's data on the player's machine: it carries the Linux build kit (kit/, made by
# tools/sorabuild/kit/mklinuxkit.py, without its wine/ folder: install.sh writes its own wine.sh for the player's Proton), the
# build scripts (tools/sorabuild: the git-tracked files minus mkzip.py's SKIP_TOOLS) and the Deck data manifest
# (tools/sorabuild/manifest.deck.json -> data-manifest.json).
# Runs ON the build machine (the Steam Deck), in a clean checkout. Not signed here: the dev PC signs the tar.gz with
# tools/installer/signrel.py (the key never leaves the PC).
#   bash tools/installer/linux/package_linux.sh <version> <out dir> --kit <linux kit> --sq <stock AppImage squashfs-root>
#        the game program from this checkout's build/linux/x86_64/game (made by the swr-build container), as in s278
#   bash tools/installer/linux/package_linux.sh <version> <out dir> --kit <linux kit> --bin-from <an unpacked Linux package>
#        the game program of an earlier Linux package byte for byte (every file checked against its manifest.json)
# Refuses: a dirty tree, a build of another commit, an exe without the stock 0.6.0-alpha.6 game code id, a sora_mod.so with
# Melty's connector in it, a data manifest that does not list every target, a kit without kit.json, a dev-machine path.
set -eu -o pipefail
V=${1:?usage: package_linux.sh <version> <out dir> --kit DIR (--sq DIR | --bin-from DIR)}
OUT=${2:?out dir}
shift 2
KIT=""; SQ=""; BINFROM=""
while [ $# -gt 0 ]; do
    case "$1" in
        --kit) KIT=$2; shift 2 ;;
        --sq) SQ=$2; shift 2 ;;
        --bin-from) BINFROM=$2; shift 2 ;;
        *) echo "package_linux: unknown option $1" >&2; exit 1 ;;
    esac
done
[ -n "$KIT" ] && [ -f "$KIT/kit.json" ] && [ -x "$KIT/python/bin/python3" ] || { echo "package_linux: --kit is not a Linux kit" >&2; exit 1; }
if { [ -n "$SQ" ] && [ -n "$BINFROM" ]; } || [ -z "$SQ$BINFROM" ]; then echo "package_linux: give --sq OR --bin-from" >&2; exit 1; fi
TREE=$(cd "$(dirname "$(readlink -f "$0")")/../../.." && pwd)
GAME_ID=ec4042eb255e81b4      # extracted/swgame.so.id that the stock 0.6.0-alpha.6 AppImage's Set up builds (AVX2 CPUs)
C=$(git -C "$TREE" rev-parse --short=7 HEAD)
[ -z "$(git -C "$TREE" status --porcelain)" ] || { echo "package_linux: $TREE is not clean" >&2; exit 1; }
# (s291 review3 F1) the kit's tools must be built from THIS tree's sources (sb.kit_stale, the check sb.kit_tool makes on the
# Deck; mkzip.kit_mismatch on Windows): a stale kit packs fine and then stops at cs_build on every Deck
STALE=$("$KIT/python/bin/python3" -E -B -c 'import sys, json; sys.path.insert(0, sys.argv[1]); import sb; print(" ".join(sb.kit_stale(json.load(open(sys.argv[2], encoding="utf-8")))))' "$TREE/tools/sorabuild" "$KIT/kit.json") || { echo "package_linux: the kit check failed to run" >&2; exit 1; }
[ -z "$STALE" ] || { echo "package_linux: the kit $KIT does not fit this tree (tool(s) built from other sources: $STALE): nothing was packed" >&2; exit 1; }
DM=$TREE/tools/sorabuild/manifest.deck.json
[ -f "$DM" ] || { echo "package_linux: no $DM" >&2; exit 1; }

N=Legend-of-Kairi-$V-linux-x86_64
P=$OUT/$N
rm -rf "$P"; mkdir -p "$P"
if [ -n "$SQ" ]; then
    G=$TREE/build/linux/x86_64/game
    D=$TREE/build/linux/x86_64/deps/lib
    case "$(cat "$G/source_id")" in "commit $C, tree "*) ;; *) echo "package_linux: the build is '$(cat "$G/source_id")', not commit $C" >&2; exit 1 ;; esac
    case "$(cat "$G/source_id")" in *modified*) echo "package_linux: the build is of a modified tree" >&2; exit 1 ;; esac
    grep -aq "$GAME_ID" "$G/savagewind" || { echo "package_linux: savagewind does not carry game code id $GAME_ID" >&2; exit 1; }
    python3 -I -B "$TREE/tools/installer/meltycheck.py" "$G/sora_mod.so"
    [ -x "$SQ/usr/bin/savagewind" ] || { echo "package_linux: $SQ is not an unpacked Savage Wind AppImage" >&2; exit 1; }
    grep -aq "0.6.0-alpha.6" "$SQ/usr/bin/savagewind" || { echo "package_linux: $SQ is not 0.6.0-alpha.6" >&2; exit 1; }
    mkdir -p "$P/bin/deps/lib"
    cp "$G/savagewind" "$G/sora_mod.so" "$P/bin/"
    for l in libSDL3.so.0 libavcodec.so.63 libavformat.so.63 libavutil.so.61 libswscale.so.10 libshaderc_shared.so.1; do
        cp -L "$D/$l" "$P/bin/deps/lib/$l"
    done
    for f in FiraSans-Medium.ttf FiraSans-SemiBold.ttf FiraSans-OFL.txt NotoSansHebrew-Medium.ttf NotoSansHebrew-OFL.txt \
             NotoSansThai-Medium.ttf NotoSansThai-OFL.txt FidelityFX-LICENSE.txt vk_pipelines.seed; do
        cp "$SQ/usr/bin/$f" "$P/bin/$f"
    done
    cp -r "$SQ/usr/bin/colour-luts" "$P/bin/colour-luts"
    cp "$SQ/usr/bin/LICENSE" "$P/bin/SavageWind-LICENSE.txt"
    SRCC="exe=$C so=$C"
else
    # the earlier package's bin/: every file = its manifest.json, and nothing else in it
    SRCC=$(python3 -I -B - "$BINFROM" "$GAME_ID" <<'EOF'
import hashlib, json, os, sys
src, gid = sys.argv[1], sys.argv[2]
m = json.load(open(os.path.join(src, "manifest.json"), encoding="utf-8"))
if m.get("game_id") != gid:
    raise SystemExit("package_linux: %s is for game code %s, not %s" % (src, m.get("game_id"), gid))
have = set()
for d, _, fs in os.walk(os.path.join(src, "bin")):
    for f in fs:
        have.add(os.path.relpath(os.path.join(d, f), os.path.join(src, "bin")).replace(os.sep, "/"))
if have != set(m["bin"]):
    raise SystemExit("package_linux: %s/bin differs from its manifest's file list" % src)
for rel, e in m["bin"].items():
    if hashlib.sha256(open(os.path.join(src, "bin", rel), "rb").read()).hexdigest() != e["sha256"]:
        raise SystemExit("package_linux: %s/bin/%s is not the one in its manifest" % (src, rel))
sc = m.get("source_commit", "")
print(" ".join(x for x in sc.split() if not x.startswith("pkg=")) + " bin-of=" + m.get("version", "?"))
EOF
)
    cp -a "$BINFROM/bin" "$P/bin"
    python3 -I -B "$TREE/tools/installer/meltycheck.py" "$P/bin/sora_mod.so"
fi
# the build kit (no wine/: its wine.sh names the kit maker's Proton, and its prefix is that machine's)
mkdir -p "$P/kit"
for e in "$KIT"/*; do
    [ "$(basename "$e")" = wine ] && continue
    cp -a "$e" "$P/kit/"
done
find "$P/kit" -name __pycache__ -type d -prune -exec rm -rf {} +
# the build scripts: the git-tracked tools/sorabuild minus mkzip.py's SKIP_TOOLS (dev configs, host tests, dev proofs)
python3 -I -B - "$TREE" "$P" <<'EOF'
import ast, os, shutil, subprocess, sys
tree, p = sys.argv[1], sys.argv[2]
src = open(os.path.join(tree, "tools", "installer", "mkzip.py"), encoding="utf-8").read()
skip = next(ast.literal_eval(n.value) for n in ast.parse(src).body
            if isinstance(n, ast.Assign) and any(getattr(t, "id", "") == "SKIP_TOOLS" for t in n.targets))
files = [f for f in subprocess.run(["git", "-C", tree, "ls-files", "-z", "tools/sorabuild"], capture_output=True,
                                   check=True).stdout.decode().split("\0") if f]
n = 0
for rel in files:
    if rel in skip:
        continue
    d = os.path.join(p, rel)
    os.makedirs(os.path.dirname(d), exist_ok=True)
    shutil.copy2(os.path.join(tree, rel), d)
    n += 1
print("package_linux: tools/sorabuild: %d files (%d skipped)" % (n, len(files) - n))
EOF
L=$TREE/tools/installer/linux
cp "$L/install.sh" "$L/update.sh" "$L/kairi.sh" "$L/README-DECK.md" "$P/"
cp -r "$L/LICENSES" "$P/LICENSES"
cp "$DM" "$P/data-manifest.json"
echo "$V" > "$P/VERSION"
chmod 755 "$P/install.sh" "$P/update.sh" "$P/kairi.sh" "$P/bin/savagewind"
chmod 644 "$P/bin/sora_mod.so"
python3 -I -B - "$P" "$V" "$C" "$GAME_ID" "$TREE" "$SRCC" <<'EOF'
import hashlib, json, os, sys, time
p, v, c, gid, tree, srcc = sys.argv[1:7]
def sha(f):
    return hashlib.sha256(open(f, "rb").read()).hexdigest()
dm = json.load(open(os.path.join(p, "data-manifest.json"), encoding="utf-8"))
tg = [l.split("#", 1)[0].split() for l in open(os.path.join(tree, "tools/sorabuild/targets.txt"), encoding="utf-8")]
tg = {t[0] for t in tg if t}
miss, extra = sorted(tg - set(dm["files"])), sorted(set(dm["files"]) - tg)
if miss or extra:
    raise SystemExit("package_linux: data manifest vs targets.txt: missing %s, extra %s" % (miss[:5], extra[:5]))
b = {}
for d, _, fs in os.walk(os.path.join(p, "bin")):
    for f in fs:
        full = os.path.join(d, f)
        b[os.path.relpath(full, p).replace(os.sep, "/")] = {"sha256": sha(full), "size": os.path.getsize(full)}
b = {k[len("bin/"):]: b[k] for k in sorted(b)}
m = {"release": "Legend of Kairi", "version": v, "track": "github-linux", "platform": "linux-x86_64",
     "made": time.strftime("%Y-%m-%d %H:%M:%S"), "source_commit": "%s pkg=%s" % (srcc, c),
     "savage_wind": "0.6.0-alpha.6", "game_id": gid,
     "data": {"build_id": dm["build_id"], "files": len(dm["files"]), "manifest": "data-manifest.json",
              "manifest_sha256": sha(os.path.join(p, "data-manifest.json")), "built_by": "install.sh (Steam Deck manifest)"},
     "kit": {"kit_json_sha256": sha(os.path.join(p, "kit", "kit.json"))},
     "bin": b}
json.dump(m, open(os.path.join(p, "manifest.json"), "w", encoding="utf-8"), indent=1)
print("package_linux: manifest: %d bin files, data %d files, build id %s, %s" % (len(b), len(dm["files"]), dm["build_id"], m["source_commit"]))
EOF
# nothing of a dev machine in our own text files (mkzip.py's USER_PATH + the Deck's home; kit/ is third-party, not scanned)
if grep -rliE "c:/users/|/home/deck" "$P" --include=*.sh --include=*.py --include=*.json --include=*.md --exclude-dir=kit; then
    echo "package_linux: the files above name a dev-machine path" >&2; exit 1
fi
tar -C "$OUT" --owner=0 --group=0 --numeric-owner --sort=name -czf "$OUT/$N.tar.gz" "$N"
echo "package_linux: $OUT/$N.tar.gz $(stat -c %s "$OUT/$N.tar.gz") bytes, sha256 $(sha256sum "$OUT/$N.tar.gz" | cut -c1-16)..., commit $C"
