# fxrec - our own KH2 effect recorder (replaces PHH's harness; s156+)

Written from scratch (no PHH code). Loads the Steam KH2 FM exe (sha1 2548F119...5A) headless and runs its effect code.

- `fxrec_ldr.c/h` - loader: maps the exe at 0x140000000 (entry point never run), binds imports (real CRT only for pure
  functions: math, rand, mem/str, heap, sprintf, _errno; throw/abort/exit = fatal; all else = counting stubs returning 0),
  fake TLS array swapped into gs:[0x58] during KH2 calls (KH2's own thread-safe statics work; only `_Init_thread_notify`
  0x439FC8 is patched to ret), .pdata registered, hooks (`mov rax,imm64; jmp rax` to a C function or to the record
  dispatcher), fault guard (VEH + RtlCaptureContext resume; stack snapshot at the fault).
- `fxrec_boot.c` - slice 1: fake allocator (bump arena, vtable slot 1) + fake device (getter 0x11C150) +
  `0x2BDB50(alloc, 0x80000, 0x20000)`. `sh build.sh && build/fxrec_boot.exe [-v] [exe path]` -> `SLICE1 PASS`.
- `fxrec_s2.c` - slice 2 (s157): `--fx <.mag/.a.us/.bar> --elem N [--frames N]` loads the file (BAR entry type 0x12 = PAX),
  PAX fix-up 0x2C4520, effect set {pax, pax+0x10, dpx}, start 0x2C6B30(set, N, 0, 1, 0, 0), then per frame 0x2BD190(proj, cam, 0)
  + 0x2BD830() and prints every draw (count/prim/stride/program/texture). Renderer wrappers run unchanged on the fake device;
  cut: 0x119BA0 + 0x123700 (D3D lambda queue), 0x11BB30 (fake ctx), 0x11A330 (draw core = snapshot), 0x121310 = 0;
  0x12C0F0 VRAM lookup = KH2's own TEX0 key -> one fake texture per key. Compare: `python -I recon/s156_work/s157_cmp.py run.txt oracle.json`.
- Build: MSYS2 UCRT64 clang; our exe is linked at 0x10000000 without ASLR so 0x140000000 stays free.
- RVAs: confirmed in recon/ghidra/kh2proj (out/s156_boot*.c, s156_d*.c). Notes: recon/S156_FXREC.md.
- Oracle (PHH output to compare against): recon/s156_work/phf2dump.py + ORACLE.md.
- slice 3 (s158): `--dump f.fxd` per-draw snapshot (vertex bytes + constant blocks + camera block), `--cam yaw,pitch`,
  `--owner x|z`, `--pre ids` (earlier elements first, like a whole-file run). `sh runcams.sh <fx> <elem> <outdir> <tag>`
  = the 5 runs; `python -E -P fxsolve.py <outdir> <tag> --out x.json` = world points + offsets + ages (fxv1); compare:
  `python -E -P recon/s156_work/fxv_cmp.py x.json recon/s156_work/oraclev_<name>.json`. Notes: recon/S156_FXREC.md "s158".
- slice 5a (s160): HD textures. `hdtab.py <KH2Extract file>` = the package's remastered table (original offset -> asset
  name; patch order kh2_first..sixth, last wins, backup/ copies = vanilla). `--texhd` / `--fxhd <tab>` answer hd_lookup
  0x12AA20 with HD texture objects; VRAM register 0x12C620 / lookup 0x12C0F0 = KH2's own map rules (magic check, exact key,
  then cbp 0, else 0; `--vramfake 1` = old fake-per-key). Run end prints `TEXID n hd <texcommon|fx> <asset> WxH`.
  runcams.sh makes the tables itself (HD=0 = without). `sh cmpall.sh` = the 12 oracle cases (want "ALL 12 MATCH");
  `python -E -P texcheck.py <kh2_fx.bin> <outdir> <cases>` = our assets' bytes vs the oracle's textures.
- slice 5b (s160): whole file. `sh runfile.sh <fx> <outdir> <tag> ["--tails 120" / "--pool 0"]` = PHH's worker order (ids
  sorted, one process per camera, 600-frame cap + kill, tails at 0..119 with loop end, cap at+300, 150 MB budget; tail
  frames before `at` stored empty) + fxsolve per recording; `sh cmpfile.sh <outdir> <oracle prefix>` vs the oracle;
  `sh allfiles.sh <list> <root> [regex]` = both for many files (fxd deleted after). `loopend.py <name>` (s160 helper) =
  does the file's script call effect_loop_end (-> --tails 120): 82/82 = oracle.
- writer (s161): `python -E -P phf2write.py --out <dir> <whole-file out dir>[=prefix] ...` = our PHF2 v4 kh2_fx.bin + kh2_fx.json
  (PAX header fields bone/flags/fade/t from D:/KH2Extract, cut from the run log, loops/tails/period like PHH, HD textures from
  KH2Extract remastered). `phf2proof.py <dir> <out dirs>` = round trip + header/json/frames/textures vs the PHH oracle (PASS on
  fire_1, thunder_1, lim_donald, prize, P_EX100, 00effect.bar); `phf2subset.py` = oracle subset for fxbake A/B. Notes: recon/S161_HELPER_WRITER.md.
