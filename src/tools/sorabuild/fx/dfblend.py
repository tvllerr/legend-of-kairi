"""(s150c) python -I dfblend.py <fx.bin> [--dry]: Duck Flare's flare BODIES are KH2 blend-2 batches (blending off, tex 240,
alpha ~224-255; recon/S150_RESEARCH_FLAREMODEL.md + s150c batch stats). runtime/sora_fx.c (exe side) skips every blend-2
batch (the s40 cure-shell rule), so the flares show only their sparkles. This flips blend 2 -> 1 (alpha pipeline) in the
batches of lim_donald2_0..5 ONLY, in place. Idempotent (a second run finds 0 to flip). Game must be CLOSED (fx.bin is mapped).
No backup (sorabuild works on a private copy)."""
import os, shutil, struct, sys

path = sys.argv[1]
dry = "--dry" in sys.argv
f = open(path, "rb" if dry else "r+b")
h = struct.unpack("<4s15I", f.read(64))
assert h[0] == b"SKFX" and h[1] == 1, "not an SKFX v1 pack"
ntex, neff, nfr, nba, nve, ntr = h[2:8]
o_eff, o_fr, o_ba = h[9], h[10], h[11]  # h[8] = textures
f.seek(o_eff); eff = f.read(64 * neff)
f.seek(o_fr); frames = f.read(8 * nfr)
todo = []  # byte offsets of the blend byte of each blend-2 batch
for i in range(neff):
    name = eff[i * 64:i * 64 + 32].split(b"\0")[0].decode()
    if not name.startswith("lim_donald2_"):
        continue
    first_frame, nframes = struct.unpack_from("<2I", eff, i * 64 + 32)
    seen = set()
    for k in range(first_frame, first_frame + nframes):
        fb, nb = struct.unpack_from("<2I", frames, k * 8)
        for b in range(fb, fb + nb):
            if b in seen:
                continue
            seen.add(b)
            f.seek(o_ba + 52 * b + 3)  # FBatch {u16 tex; u8 flags, blend, ...}
            if f.read(1)[0] == 2:
                todo.append(o_ba + 52 * b + 3)
    print("%s: %d batches" % (name, len(seen)))
print("blend-2 batches to flip: %d%s" % (len(todo), " (dry run)" if dry else ""))
if todo and not dry:
    f.close()
    with open(path, "r+b") as g:
        for off in todo:
            g.seek(off); g.write(b"\x01")
    print("flipped %d batches 2 -> 1" % len(todo))
