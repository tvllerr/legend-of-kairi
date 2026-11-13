#!/bin/bash
# Builds only build/sora_mod.dll (Sora's hot-reloaded driver) of the tree this script is in, with the Makefile's own
# rule and flags (with the Melty connector when runtime/melty/ is there). Never relinks savagewind.exe: the rule's
# order-only prerequisite on it is held with `make -o` (the running game keeps the .exe locked, and its link is long).
# Needs build/savagewind.dll.a, the exe's import library (made by a full `make`).
# Extra arguments go to make, e.g. NO_MELTY=1 (build without the connector) or -j2. B= (another build dir): not supported.
export PATH=/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH
TREE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd) || exit 1
cd "$TREE" || exit 1
if [ ! -f build/savagewind.dll.a ]; then
    echo "make_sora_dll: FAILED, no build/savagewind.dll.a in $TREE (build the exe once first: make)" >&2
    exit 1
fi
melty=OFF
if [ -f runtime/melty/sora_melty.cpp ]; then melty=ON; fi
for a in "$@"; do case "$a" in NO_MELTY=?*) melty=OFF ;; esac; done
before=$(stat -c %Y build/sora_mod.dll 2>/dev/null)
if ! make -o build/savagewind.exe "$@" build/sora_mod.dll; then
    echo "make_sora_dll: FAILED ($TREE)" >&2
    exit 1
fi
after=$(stat -c %Y build/sora_mod.dll 2>/dev/null)
if [ -z "$after" ]; then echo "make_sora_dll: FAILED, no build/sora_mod.dll ($TREE)" >&2; exit 1; fi
if [ "$before" = "$after" ]; then
    echo "sora_mod.dll rebuilt: nothing to do, already up to date ($TREE, Melty connector $melty)"
else
    echo "sora_mod.dll rebuilt ($TREE, Melty connector $melty)"
fi
