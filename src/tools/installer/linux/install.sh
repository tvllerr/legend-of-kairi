#!/bin/bash
# Legend of Kairi (KH2 Sora in Breath of the Wild) - Linux / Steam Deck installer, release 2.
# Builds the mod's data ON THIS MACHINE from your Steam KINGDOM HEARTS II (about 20-25 minutes on a Steam Deck) and checks
# every file against the list made on a Steam Deck. README-DECK.md has the steps.
#
#   ./install.sh                      asks for the folders it cannot find by itself
#   ./install.sh [--dir DIR] [--botw DIR] [--kh2 DIR] [--proton DIR] [--appimage FILE] [--yes]
#     --dir       the install folder (default ~/LegendOfKairi). An existing install there is UPDATED: saves, settings and
#                 the Savage Wind game files are kept; the old data stays in use until the new data is built and checked.
#     --botw      your BotW 1.9.0 dump: a folder with exefs/ and romfs/ (an existing Savage Wind "extracted" folder works too).
#                 Only needed the first time (or when Savage Wind's game files must be set up again).
#     --kh2       your Steam "KINGDOM HEARTS -HD 1.5+2.5 ReMIX-" folder (found by itself in your Steam library folders).
#                 It must be UNMODDED: KH2 mods that change the kh2_*.pkg files are refused. It is only read, never changed.
#     --proton    a Steam Proton folder (found by itself, Proton 11.0 first): its wine runs one Windows tool of the build.
#     --appimage  a local copy of the stock Savage_Wind-0.6.0-alpha.6-x86_64.AppImage instead of the download (checked the same).
#     --yes       no questions: stop with a message when a folder is missing.
# Nothing here needs root. Nothing outside the install folder is changed (no Steam files: you add the Steam shortcut yourself).
set -u -o pipefail
umask 022
HERE=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
VERSION=$(cat "$HERE/VERSION" 2>/dev/null)
SW_WANT=0.6.0-alpha.6
AI_NAME=Savage_Wind-0.6.0-alpha.6-x86_64.AppImage
AI_URL=https://github.com/Revivedx/SavageWindRecomp/releases/download/v0.6.0-alpha.6/$AI_NAME
AI_SIZE=87185912
AI_SHA=b063116f5ee57515ebbcad12aa276b194a8e349bb44d507ced8417df4b6d8fd7
KH2_DIRNAME="KINGDOM HEARTS -HD 1.5+2.5 ReMIX-"
KH2_EXE="KINGDOM HEARTS II FINAL MIX.exe"
KH2_EXE_SHA1=2548f1191fe6587a34947952763ce6bb4f3b5c1a

die() { echo; echo "STOPPED: $*" >&2; [ -n "${LOG:-}" ] && echo "[install] STOPPED: $*" >> "$LOG"; exit 1; }
note() { echo "$*"; [ -n "${LOG:-}" ] && echo "[install] $*" >> "$LOG"; }

ROOT="$HOME/LegendOfKairi"; BOTW=""; KH2=""; PROTON=""; AIFILE=""; YES=0
while [ $# -gt 0 ]; do
    case "$1" in
        --dir) ROOT=$2; shift 2 ;;
        --botw) BOTW=$2; shift 2 ;;
        --kh2) KH2=$2; shift 2 ;;
        --proton) PROTON=$2; shift 2 ;;
        --appimage) AIFILE=$2; shift 2 ;;
        --data) die "--data is gone: this release builds the mod's data itself from your Steam KINGDOM HEARTS II (see --help)" ;;
        --yes|-y) YES=1; shift ;;
        -h|--help) sed -n '2,19p' "$0"; exit 0 ;;
        *) die "unknown option $1 (see ./install.sh --help)" ;;
    esac
done
ask() {  # ask <question> -> REPLY (a path; ~ expanded)
    [ $YES = 1 ] && die "$1 (pass it as an option, see --help)"
    printf '%s\n> ' "$1"; read -r REPLY || die "no answer"
    REPLY=${REPLY/#\~/$HOME}; REPLY=${REPLY%/}
}

echo "== Legend of Kairi $VERSION for Linux / Steam Deck (single player, Savage Wind $SW_WANT)"
# ---- 1. checks
[ -n "$VERSION" ] && [ -f "$HERE/manifest.json" ] && [ -f "$HERE/data-manifest.json" ] && [ -d "$HERE/bin" ] \
    && [ -x "$HERE/kit/python/bin/python3" ] && [ -f "$HERE/tools/sorabuild/sorabuild.py" ] \
    || die "run install.sh from the unpacked Legend of Kairi folder (it needs its bin/, kit/ and tools/ folders)"
[ "$(uname -m)" = x86_64 ] || die "this release is for x86_64 Linux only"
[ "$(id -u)" != 0 ] || die "do not run this as root (no sudo needed)"
for t in python3 sha1sum sha256sum df comm; do command -v $t >/dev/null 2>&1 || die "'$t' is missing on this system"; done
ldconfig -p 2>/dev/null | grep -q 'libvulkan\.so\.1' || [ -e /usr/lib/libvulkan.so.1 ] || [ -e /usr/lib64/libvulkan.so.1 ] \
    || die "no Vulkan loader (libvulkan.so.1) found: the game needs Vulkan"
ROOT=${ROOT/#\~/$HOME}; ROOT=${ROOT%/}
case "$ROOT" in /*) ;; *) ROOT="$PWD/$ROOT" ;; esac
[ "$ROOT" != "$HERE" ] || die "install into another folder than the unpacked one (default ~/LegendOfKairi)"
case "$ROOT" in "$HERE"/*) die "the install folder cannot be inside the unpacked folder" ;; esac
mkdir -p "$ROOT/logs" "$ROOT/dl" || die "cannot create $ROOT"
LOG="$ROOT/logs/install.log"
echo "[install] $(date) Legend of Kairi $VERSION from $HERE into $ROOT" >> "$LOG"
# one installer at a time; never while this install's game runs
LOCK="$ROOT/.install.lock"
if [ -f "$LOCK" ] && kill -0 "$(cat "$LOCK" 2>/dev/null)" 2>/dev/null; then die "another install.sh is running on $ROOT"; fi
echo $$ > "$LOCK"; trap 'rm -f "$LOCK"' EXIT
pgrep -f "^$ROOT/bin/savagewind" >/dev/null 2>&1 && die "Legend of Kairi is running from $ROOT: close the game first"
GAME_ID=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["game_id"])' "$HERE/manifest.json") || die "manifest.json unreadable"
BUILD_ID=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["data"]["build_id"])' "$HERE/manifest.json") || die "manifest.json unreadable"
OLD=""; [ -f "$ROOT/VERSION" ] && OLD=$(cat "$ROOT/VERSION")
if [ -n "$OLD" ]; then note "updating the install in $ROOT ($OLD -> $VERSION): saves and settings are kept"; else note "new install in $ROOT"; fi
free_gb() { df -Pk "$1" | awk 'NR==2 {printf "%d", $4/1048576}'; }
KIT="$HERE/kit"
KPY="$KIT/python/bin/python3"
SB="$HERE/tools/sorabuild"
BUILD="$ROOT/build"            # the data build's scratch: out/ (the new data), work/, the wine prefix. Gone after a good build.
# A failed KH2 check (before any long step) removes build/ again, but only when THIS run made it. After a failed data build it
# must stay: the next run goes on where it stopped (and bad.txt tells a rebuild from a machine that builds differently).
drop_build() { [ "${BUILD_NEW:-0}" = 1 ] && [ "$BUILD" = "$ROOT/build" ] && rm -rf -- "$BUILD"; return 0; }
OLDOK=""; [ -n "$OLD" ] && OLDOK=" Your $OLD install still works as it was."

# ---- 2. is the installed data already this version's? (a re-run: then no KH2 / Proton / build is needed)
check_data() {  # check_data <dir> : every file of data-manifest.json right (size + sha256)? exit 0 = yes
    python3 -I -B - "$HERE/data-manifest.json" "$1" <<'EOF'
import hashlib, json, os, sys
man, dst = sys.argv[1], sys.argv[2]
files = json.load(open(man, encoding="utf-8"))["files"]
def good(rel, e):
    p = os.path.join(dst, *rel.split("/"))
    if not os.path.isfile(p) or os.path.getsize(p) != e["size"]:
        return False
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest() == e["sha256"]
bad = [rel for rel, e in sorted(files.items()) if not good(rel, e)]
print("[data] %d files, %d wrong or missing%s" % (len(files), len(bad), (": " + ", ".join(bad[:8])) if bad else ""))
sys.exit(1 if bad else 0)
EOF
}
NEED_BUILD=1
if [ -d "$ROOT/data" ] && [ "$(cat "$ROOT/data/BUILD_ID" 2>/dev/null)" = "$BUILD_ID" ]; then
    note "checking the installed data..."
    check_data "$ROOT/data" | tee -a "$LOG" && NEED_BUILD=0
fi

if [ $NEED_BUILD = 1 ]; then
    # ---- 3. your Steam KINGDOM HEARTS II (only read) + a Proton (its wine runs fxrec_boot.exe, one Windows tool of the build)
    STEAMS=$(python3 -I -B - <<'EOF'
import os, re
home = os.path.expanduser("~")
libs = []
for r in (".local/share/Steam", ".steam/steam", ".steam/root", ".var/app/com.valvesoftware.Steam/.local/share/Steam"):
    r = os.path.join(home, r)
    vdf = os.path.join(r, "steamapps", "libraryfolders.vdf")
    more = re.findall(r'"path"\s+"([^"]+)"', open(vdf, errors="replace").read()) if os.path.isfile(vdf) else []
    for l in [r] + more:
        l = os.path.realpath(l)
        if l not in libs and os.path.isdir(os.path.join(l, "steamapps")):
            libs.append(l)
print("\n".join(libs))
EOF
)
    if [ -z "$KH2" ] && [ -f "$ROOT/kh2.path" ]; then KH2=$(cat "$ROOT/kh2.path"); note "KINGDOM HEARTS: the folder of the last install ($KH2)"; fi
    if [ -z "$KH2" ]; then
        while IFS= read -r lib; do
            [ -n "$lib" ] && [ -f "$lib/steamapps/common/$KH2_DIRNAME/$KH2_EXE" ] && { KH2="$lib/steamapps/common/$KH2_DIRNAME"; break; }
        done <<< "$STEAMS"
        [ -n "$KH2" ] && note "KINGDOM HEARTS: found $KH2"
    fi
    if [ -z "$KH2" ]; then ask "Your Steam KINGDOM HEARTS HD 1.5+2.5 ReMIX folder (it has \"$KH2_EXE\" and Image/dt):"; KH2=$REPLY; fi
    KH2=$(readlink -f "$KH2") || die "no such folder"
    case "$(basename "$KH2")" in dt) KH2=$(dirname "$(dirname "$KH2")") ;; Image) KH2=$(dirname "$KH2") ;; esac
    [ -f "$KH2/$KH2_EXE" ] && [ -f "$KH2/Image/dt/kh2_first.hed" ] \
        || die "$KH2 is not the KINGDOM HEARTS HD 1.5+2.5 ReMIX folder (no $KH2_EXE / Image/dt). Pass it with --kh2 DIR."
    [ "$(sha1sum "$KH2/$KH2_EXE" | cut -d' ' -f1)" = $KH2_EXE_SHA1 ] \
        || die "your $KH2_EXE is not the Steam release this mod is made for (changed by a mod, or the Epic / another version: not supported yet).$OLDOK"
    if [ -z "$PROTON" ]; then
        PROTON=$(python3 -I -B - "$STEAMS" <<'EOF'
import os, re, sys
cands = []
for lib in [l for l in sys.argv[1].split("\n") if l]:
    for base in (os.path.join(lib, "steamapps", "common"), os.path.join(lib, "compatibilitytools.d")):
        if not os.path.isdir(base):
            continue
        for n in os.listdir(base):
            d = os.path.join(base, n)
            if "proton" in n.lower() and os.path.isfile(os.path.join(d, "files", "bin", "wine")):
                m = re.fullmatch(r"Proton (\d+)\.(\d+)", n)
                # Proton 11.0 = the one the Deck data was made + checked with; then newer numbered ones, Experimental, the rest
                rank = (0, 0, 0) if n == "Proton 11.0" else (1, -int(m.group(1)), -int(m.group(2))) if m else \
                       (2, 0, 0) if n == "Proton - Experimental" else (3, 0, 0)
                cands.append((rank, n, d))
print(sorted(cands)[0][2] if cands else "")
EOF
)
        [ -n "$PROTON" ] && note "Proton: found $PROTON"
    fi
    if [ -z "$PROTON" ]; then
        echo "No Steam Proton found. In Steam: Library > search \"Proton 11.0\" > Install, then run this again (or pass --proton DIR)."
        ask "A Proton folder (it has files/bin/wine):"; PROTON=$REPLY
    fi
    PROTON=$(readlink -f "$PROTON") || die "no such folder"
    [ -x "$PROTON/files/bin/wine" ] || die "$PROTON is not a Proton folder (no files/bin/wine)"
    case "$(basename "$PROTON")" in "Proton 11.0") ;; *) note "(the data was checked with Proton 11.0; $(basename "$PROTON") should work the same)" ;; esac

    BUILD_NEW=0; [ -e "$BUILD" ] || BUILD_NEW=1     # 1 = this run makes the build folder (only then may a failed KH2 check remove it)
    mkdir -p "$BUILD/out" "$BUILD/work" || die "cannot write in $BUILD"
    cat > "$BUILD/wine.sh" <<EOF
#!/bin/sh
# (written by install.sh) start a Windows exe of the build (fxrec_boot.exe) with Proton's wine; own prefix, no Steam needed
export WINEPREFIX="$BUILD/pfx" WINEDEBUG="\${WINEDEBUG:--all}" WINEDLLOVERRIDES="\${WINEDLLOVERRIDES:-mscoree,mshtml=}"
exec "$PROTON/files/bin/wine" "\$@"
EOF
    chmod +x "$BUILD/wine.sh"
    python3 -I -B - "$BUILD/config.json" "$KIT" "$KH2" "$KH2_EXE" "$ROOT/sw/extracted/romfs" "$BUILD" <<'EOF' || die "cannot write $BUILD/config.json"
import json, os, sys
p, kit, kh2, exe, romfs, b = sys.argv[1:7]
json.dump({"kit": kit, "kh2img": os.path.join(kh2, "Image", "dt"), "kh2exe": os.path.join(kh2, exe), "romfs": romfs,
           "out": os.path.join(b, "out"), "work": os.path.join(b, "work"), "wine": os.path.join(b, "wine.sh"),
           "fx": "fxrec", "sounds": "recipe", "fx_keep": "used"}, open(p, "w", encoding="utf-8"), indent=1)
EOF
    # the KH2 files first (a minute or two): a modded KH2 is refused here, before the long steps
    note "reading the KINGDOM HEARTS II files the mod needs (only read: nothing in your KH2 folder is changed)..."
    ( cd "$SB" && LANG=C.UTF-8 "$KPY" -E -P sorabuild.py --config "$BUILD/config.json" ) > "$ROOT/logs/kh2pick.log" 2>&1
    rc=$?
    grep -a "kh2pick" "$ROOT/logs/kh2pick.log" >> "$LOG"
    if grep -aq "kh2pick: your KH2 files differ" "$ROOT/logs/kh2pick.log"; then
        drop_build
        die "your KINGDOM HEARTS II files are MODDED (a KH2 mod changed the kh2_*.pkg files in $KH2/Image/dt), or not the Steam release.
Legend of Kairi is built from the unmodded Steam files. Nothing was changed in your KH2 folder.$OLDOK
To go on: uninstall that KH2 mod (or Steam > KINGDOM HEARTS HD 1.5+2.5 ReMIX > Properties > Installed Files > Verify
integrity, which removes KH2 mods), or point install.sh at an unmodded copy of the folder with --kh2 DIR.
Installed 0.2.1 with update.sh? Then run:  $ROOT/update.sh --kh2 DIR   (DIR = the clean KINGDOM HEARTS folder;
add --force if it says you are up to date)."
    fi
    [ $rc = 0 ] || { drop_build; die "reading the KINGDOM HEARTS II files failed (exit $rc, log: $ROOT/logs/kh2pick.log).$OLDOK"; }
    echo "$KH2" > "$ROOT/kh2.path"
fi

# ---- 4. Savage Wind's game files (sw/): the stock AppImage's own Set up, run once without its window, into sw/
SW="$ROOT/sw"
sw_ready() { [ -d "$SW/extracted/exefs" ] && [ -d "$SW/extracted/romfs" ] && [ "$(cat "$SW/extracted/swgame.so.id" 2>/dev/null)" = "$GAME_ID" ]; }
if sw_ready; then
    note "Savage Wind game files: ready (game code $GAME_ID)"
else
    SRC=""   # "" = the game files are copied already: Set up only builds the game code again
    if [ -n "$BOTW" ] || [ ! -d "$SW/extracted/exefs" ] || [ ! -d "$SW/extracted/romfs" ]; then
        if [ -z "$BOTW" ]; then ask "Folder of your BotW 1.9.0 dump (it has exefs/ and romfs/):"; BOTW=$REPLY; fi
        BOTW=$(readlink -f "$BOTW") || die "no such folder"
        [ -d "$BOTW" ] || die "no such folder: $BOTW"
        [ -n "$(find "$BOTW" -maxdepth 3 -type d -name romfs 2>/dev/null | head -n 1)" ] || die "no romfs/ folder in $BOTW"
        case "$BOTW" in "$SW"|"$SW"/*) die "the BotW dump cannot be inside $SW" ;; esac
        SRC=$BOTW
        # Set up copies about 15 GB (hard links when the dump is on the same disk as the install)
        need=18; [ "$(stat -c %d "$BOTW")" = "$(stat -c %d "$ROOT")" ] && need=3
    else
        need=2
    fi
    [ $NEED_BUILD = 1 ] && need=$((need + 10))     # + the data build (out 3.1 GB, work, the KH2 files)
    [ "$(free_gb "$ROOT")" -ge $need ] || die "not enough free space in $ROOT ($(free_gb "$ROOT") GB free, about $need GB needed)"
    if [ -z "$AIFILE" ]; then
        AIFILE="$ROOT/dl/$AI_NAME"
        if [ ! -f "$AIFILE" ] || [ "$(stat -c %s "$AIFILE")" != $AI_SIZE ]; then
            command -v curl >/dev/null 2>&1 || die "'curl' is missing (or pass --appimage FILE)"
            note "downloading Savage Wind $SW_WANT (83 MB) from the Savage Wind GitHub releases..."
            curl -fL --progress-bar --proto '=https' --max-filesize 100000000 -o "$AIFILE.part" "$AI_URL" || die "the download failed"
            mv -f "$AIFILE.part" "$AIFILE"
        fi
    fi
    [ "$(stat -c %s "$AIFILE" 2>/dev/null)" = $AI_SIZE ] && [ "$(sha256sum "$AIFILE" | cut -d' ' -f1)" = $AI_SHA ] \
        || die "$AIFILE is not the stock Savage Wind $SW_WANT AppImage (size / sha256 differ)"
    note "unpacking the AppImage (no FUSE needed)..."
    rm -rf "$ROOT/dl/sw-appimage"; mkdir -p "$ROOT/dl/sw-appimage"
    cp -f "$AIFILE" "$ROOT/dl/sw-appimage/sw.AppImage" && chmod +x "$ROOT/dl/sw-appimage/sw.AppImage"
    (cd "$ROOT/dl/sw-appimage" && ./sw.AppImage --appimage-extract >/dev/null) || die "the AppImage did not unpack"
    rm -f "$ROOT/dl/sw-appimage/sw.AppImage"
    [ -x "$ROOT/dl/sw-appimage/squashfs-root/AppRun" ] || die "the AppImage did not unpack (no AppRun)"
    note "Savage Wind Set up: copying the game files and building its game code (about 10-20 minutes, a window may open)..."
    mkdir -p "$SW"
    ( export SAVAGEWIND_DATA_ROOT="$SW" SAVAGEWIND_LAUNCHER_EXTRACT="$SRC" SAVAGEWIND_NO_UPDATE_CHECK=1
      [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] || export DISPLAY=:0
      [ -n "${XDG_RUNTIME_DIR:-}" ] || export XDG_RUNTIME_DIR=/run/user/$(id -u)
      "$ROOT/dl/sw-appimage/squashfs-root/AppRun" ) > "$ROOT/logs/sw-setup.log" 2>&1
    grep -a "\[launcher\] setup:" "$ROOT/logs/sw-setup.log" | tail -n 1 | tee -a "$LOG"
    sw_ready || die "Savage Wind's Set up did not finish (game code '$(cat "$SW/extracted/swgame.so.id" 2>/dev/null)', want $GAME_ID). Log: $ROOT/logs/sw-setup.log"
    echo "$SW_WANT" > "$SW/KAIRI_SW_VERSION"
    rm -rf "$ROOT/dl/sw-appimage"
    note "Savage Wind game files: ready (game code $GAME_ID)"
fi

# ---- 5. the mod's data: built into build/out (the installed data/ is not touched), then checked file by file
if [ $NEED_BUILD = 1 ]; then
    [ -f "$SW/extracted/romfs/Pack/Bootup.pack" ] || die "Savage Wind's game files have no romfs/Pack/Bootup.pack (run install.sh --botw DIR again)"
    [ "$(free_gb "$ROOT")" -ge 8 ] || die "not enough free space in $ROOT ($(free_gb "$ROOT") GB free, the data build needs about 8 GB)"
    # keep the machine awake during the build (a Steam Deck that sleeps stops it); idle-only when sleep cannot be held
    INH=""
    if command -v systemd-inhibit >/dev/null 2>&1; then
        for w in sleep:idle idle; do
            if systemd-inhibit --what=$w --who=Legend_of_Kairi --why=test true >/dev/null 2>&1; then
                INH="systemd-inhibit --what=$w --who=Legend_of_Kairi --why=building_the_mod_data"; break
            fi
        done
    fi
    note "building the mod's data from your KINGDOM HEARTS II (about 20-25 minutes on a Steam Deck: keep it PLUGGED IN)..."
    [ -n "$OLD" ] && note "(your $OLD install keeps working until the new data is built and checked)"
    t0=$(date +%s)
    ( cd "$SB" && LANG=C.UTF-8 $INH "$KPY" -E -P sorabuild.py --config "$BUILD/config.json" all ) 2>&1 \
        | tee "$ROOT/logs/build.log" | grep -a --line-buffered -E '^\[(progress|sorabuild)\]'
    rc=${PIPESTATUS[0]}
    [ $rc = 0 ] || die "the data build stopped (exit $rc). Run install.sh again: it goes on where it stopped. Log: $ROOT/logs/build.log.$OLDOK"
    note "data built in $(( ($(date +%s) - t0) / 60 )) min; checking every file..."
    ( cd "$SB" && "$KPY" -E -P verify.py --out "$BUILD/out" --expect "$HERE/data-manifest.json" ) > "$ROOT/logs/verify.log" 2>&1
    rc=$?
    grep -av " SAME$" "$ROOT/logs/verify.log" | tee -a "$LOG" | tail -n 12
    if [ $rc != 0 ]; then
        # the wrong files are deleted: the next run rebuilds only their steps. The same files wrong after a rebuild = this
        # machine builds them differently (not a broken file)
        grep -aE "(DIFF|MISSING|NOT IN)" "$ROOT/logs/verify.log" | awk '{print $2}' | sort -u > "$BUILD/bad.new"
        while IFS= read -r f; do [ -n "$f" ] && rm -f "$BUILD/out/$f"; done < "$BUILD/bad.new"
        if [ -s "$BUILD/bad.new" ] && [ -f "$BUILD/bad.txt" ] && [ -z "$(comm -23 "$BUILD/bad.new" "$BUILD/bad.txt")" ]; then
            die "$(wc -l < "$BUILD/bad.new") data file(s) came out different again after a rebuild: this machine builds them
differently from a Steam Deck (other Linux PCs are not proven yet). Please send $ROOT/logs (build.log, verify.log) to Tyler.$OLDOK"
        fi
        mv -f "$BUILD/bad.new" "$BUILD/bad.txt"
        die "$(wc -l < "$BUILD/bad.txt") data file(s) did not come out right: run install.sh again to rebuild them (log: $ROOT/logs/verify.log).$OLDOK"
    fi
    grep -aq "build id $BUILD_ID" "$ROOT/logs/verify.log" || die "the check did not print build id $BUILD_ID (log: $ROOT/logs/verify.log).$OLDOK"
    note "data: every file right (build id $BUILD_ID)"
fi

# ---- 6. the game program (bin/), checked against manifest.json; then the new data + bin go in together
note "installing the game program..."
rm -rf "$ROOT/bin.new" "$ROOT/bin.old" "$ROOT/data.old"
cp -a "$HERE/bin" "$ROOT/bin.new" || die "copy of bin/ failed"
python3 -I -B - "$HERE/manifest.json" "$ROOT/bin.new" <<'EOF' || die "the game program files do not match manifest.json (a damaged download?)"
import hashlib, json, os, sys
m = json.load(open(sys.argv[1], encoding="utf-8"))["bin"]
bad = []
for rel, e in m.items():
    p = os.path.join(sys.argv[2], *rel.split("/"))
    if not os.path.isfile(p) or hashlib.sha256(open(p, "rb").read()).hexdigest() != e["sha256"]:
        bad.append(rel)
print("[bin] %d files, %d wrong%s" % (len(m), len(bad), (": " + ", ".join(bad[:5])) if bad else ""))
sys.exit(1 if bad else 0)
EOF
if [ $NEED_BUILD = 1 ]; then
    if [ -d "$ROOT/data" ]; then mv "$ROOT/data" "$ROOT/data.old" || die "could not move the old data aside"; fi
    if ! mv "$BUILD/out" "$ROOT/data"; then
        [ -d "$ROOT/data.old" ] && mv "$ROOT/data.old" "$ROOT/data"
        die "could not put the new data in place"
    fi
fi
[ -d "$ROOT/bin" ] && mv "$ROOT/bin" "$ROOT/bin.old"
mv "$ROOT/bin.new" "$ROOT/bin" || die "could not put the new bin/ in place"
# (a new file + rename, never written in place: update.sh may be the script running this one)
for f in manifest.json data-manifest.json VERSION README-DECK.md kairi.sh update.sh; do
    cp -f "$HERE/$f" "$ROOT/$f.new" && mv -f "$ROOT/$f.new" "$ROOT/$f" || die "copy of $f failed"
done
rm -rf "$ROOT/LICENSES"; cp -a "$HERE/LICENSES" "$ROOT/LICENSES"
chmod +x "$ROOT/kairi.sh" "$ROOT/update.sh" "$ROOT/bin/savagewind"
echo "$GAME_ID" > "$ROOT/data/GAME_ID"
echo "$BUILD_ID" > "$ROOT/data/BUILD_ID"
rm -rf "$ROOT/bin.old" "$ROOT/data.old" "$BUILD"
note "done: Legend of Kairi $VERSION in $ROOT"
if [ -n "$OLD" ]; then echo "Updated. Play it as before (your Steam shortcut to $ROOT/kairi.sh)."; exit 0; fi
cat <<EOF

=== Legend of Kairi $VERSION is installed in $ROOT ===
To play in Game Mode (first time only), in Desktop Mode:
  1. Open Steam > Games > "Add a Non-Steam Game to My Library..." > Browse...
  2. Pick:  $ROOT/kairi.sh   (file type "All Files"), then "Add Selected Programs".
  3. Do NOT set a compatibility tool (Proton) for it: it is a native Linux game.
  4. Back to Game Mode > Library > Non-Steam > kairi.sh (rename it "Legend of Kairi" in its Properties).
The first start waits about a minute once (shaders). Saves + settings: $ROOT/sw (save/, savagewind.ini).
Update later: run $ROOT/update.sh in Konsole.
EOF
