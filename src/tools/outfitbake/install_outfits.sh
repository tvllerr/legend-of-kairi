#!/bin/sh
# (s146 outfit) installs the baked outfit bodies into D:/SoraBotW/data/outfits (28 files, ~160 MB). They are NEW files that the DLL
# reads lazily by path, so the game may stay open (an existing file of the same name is backed up as <file>.pre-s146 first, and a
# running game that already loaded it keeps its copy). Rebuild them with tools/outfitbake/bake_outfits.sh (needs tools/sorabake/out/sorabake.exe).
SRC=${SRC:-D:/SoraBotW/src/wt146-outfit/data/outfits}
DST=D:/SoraBotW/data/outfits
mkdir -p "$DST" || exit 1
n=0
for f in "$SRC"/*_model.bin; do
  b=$(basename "$f")
  if [ -f "$DST/$b" ] && ! cmp -s "$f" "$DST/$b"; then cp -p "$DST/$b" "$DST/$b.pre-s146"; fi
  cp -f "$f" "$DST/$b" && n=$((n+1))
done
echo "installed $n outfit models into $DST"
