# sorabuild - one data builder (friend installer W1)

Rebuilds every data file the mod reads (targets.txt) from the player's OWN files: KH2 extract, BotW romfs (and, only for the old `"fx": "phh"` mode, a PHH 0.4.8 cache = config `fxcache`). Ships only our code; no game data in git.

    python -E -P sorabuild.py --config config.thispc.json all          # build what is not done yet
    python -E -P sorabuild.py --config config.thispc.json --list       # steps + done state
    python -E -P verify.py --out D:/SoraBotW/sb/out --ref D:/SoraBotW/data [--group fx] [--manifest manifest.json]

- `-E -P` (not `-I`): same isolation of the script dir, but numpy/Pillow in the user site-packages stay visible.
- Steps: `steps/<group>.py`, `STEPS = [Step(name, run, outputs=[...], deps=[...], work_outputs=[...])]` (see sb.py).
  Groups: fx (fx.bin + sfx.bin chain), bakes (C# model/pose/motion/hit bakers), hud (HUD bakes), levels, ours.
- Tool sources used by steps live under tools/sorabuild/<group>/ (copied out of the old tools_sNNN folders and
  parameterised: no D:/ or C:/ literal anywhere; every path comes from ctx).
- W1 is done when `verify.py` says SAME for every target on this PC, starting from an EMPTY out + work dir.

## Needs (this PC has all of them)
- Python 3.12 + numpy, Pillow, oead (`pip install numpy pillow oead`); .NET 8 SDK; OpenKH release DLLs (2-1691);
  ffmpeg (sfx_skate resample; matched with gyan 9.0.2 full_build - pin it for friends, review S1).
- fx/sfx steps are ONE chain (sorabuild re-runs all of it if any part must run); a re-run step drops its dependents' done markers.
- Inputs: KH2 extract (OpenKH `OpenKh.Command.IdxImg hed extract <kh2_*.hed> -o <dir>/<kh2_name>`), the BotW romfs (config `fxcache` only with `"fx": "phh"`; `phh` is the deprecated old key name, `--phh` / `SB_PHH` likewise).

## Status (s154)
| group | files | state |
|---|---|---|
| hud | 14 | byte-identical from empty dirs (hudbake merged: s119 lineage + camp/campseq of the Oct 30 lineage) |
| levels | 1 | byte-identical (gdt_names.py: the s32 rule = every [A-Za-z0-9_.-]{3,} run in the 27 bgdata files) |
| fx, bakes | 3, 119 | helpers b154-fxsfx / b154-bakes |

## Sounds without the PHH cache (s156, route B)
Config `"sounds": "recipe"` (the default since s163; `"phh"` = old PHH-cache behaviour, explicit opt-in only) makes the sfx tools read `work/sounds/` instead of the PHH
cache. The step `sounds_build` (steps/sounds.py) fills it from `work/sounds_recipe.json` (numbers and names: sound ids,
KH2 SCD file + audio stream, codec, volume, trigger frames, vtbl/effect/system sound ids, two mset trigger tables, hud advance
widths; no audio, no game bytes) and the player's KH2 extract (`sounds/build_sounds.py`, our MS-ADPCM decoder fx/mkextra.py).
- (s182) The recipe is MINED on the player's PC by the step `sounds_mine` (`sounds/mine/mine_recipe.py`: actors + motions from the
  msets / 00battle / weapon banks / voice SCDs, effects from the fxrec effect files, system + hud from our names + fontinfo.bar + the
  KH2 exe (config `kh2exe`), sounds = what our sfx tools ask for, traced on silent wavs). Needs fx_trigjson + the summon bakes, never
  the recipe itself. Every section equals the old frozen file (now `sounds/dev/recipe_reference.json`, dev only, not shipped;
  compare with `sounds/mine/cmp_part.py`) except se000 #306 (Roxas' run, which the old table lacked; vanilla KH2 plays it, so it
  is in since s182: 466 sounds); the "sounds" entry order differs too (nothing reads it).
- Ogg Vorbis voices (133 wavs) need vgmstream-cli r2117: `python -E -P sounds/fetch_vgmstream.py` (official release zip, sha256
  pinned, unpacks to the gitignored `sounds/bin/`), or config `"vgmstream": "<path>"` (exe sha256 pinned in build_sounds.py VGM_KNOWN).
  s157 proof: all 465 wavs (332 ADPCM + 133 Vorbis) byte-identical to the PHH cache; recipe chain fx.bin + sfx.bin + 025.wav + hud SAME.
  The old `"vorbis_fallback": "phh"` copy route was removed (s163): sorabuild / build_sounds refuse it.
- The recipe holds the sha256 of its 23 source SCDs: a different KH2 build is refused (build_sounds --no-scd-check = dev only).
- `fx/sndsrc.py` is the one place that knows where the tools read sounds (env SB_SNDROOT; SB_WAVTRACE logs the wavs opened).
- `sounds/dev/gen_recipe.py` (RETIRED s182, dev only, not shipped) made the old frozen recipe from a PHH cache + a wav trace.
- Switching the mode, changed miner / sfx tool code (sounds_mine_mode.txt) or a new recipe (sounds_mode.txt = mode + recipe sha256) re-runs
  `sounds_mine` / `sounds_build` and everything after them by itself. kh2_fx.* (fx_base and the fx tools) read the fx cache (config `fxcache`, or work/fxrec/out with `"fx": "fxrec"`).

## fx without the PHH cache (s161, switch "fx")
Config `"fx": "fxrec"` (default `"phh"` = old behaviour) makes the fx tools read kh2_fx.bin / kh2_fx.json from `work/fxrec/out/` (env SB_FXDIR)
instead of the PHH cache. Steps `fxrec_build` (clang -> work/fxrec/build/fxrec_boot.exe) and `fxrec_record` (steps/fxrec.py ->
fxrec/recordall.py: 82 files of fxrec/files82.txt, `--jobs` at once, loopend.py decides the tails, then phf2write.py). Needs config
`"kh2exe"` (Steam KH2 FM exe, sha1 2548F119...5A, others refused), `"kh2"`; optional `fxrec_jobs` (default 2), `clang`, `bash`.
With `"sounds": "recipe"` too, the PHH cache is not needed at all. Full run: `sorabuild.py --config <cfg> fxrec_record` (~1-2 h, resumable per file).
Test filter `"fxrec_only": "<regex over file names>"` = subset recording; the fx chain refuses to use it.

## Friend kit (s168): no SDK / clang / MSYS2 / Git bash / system Python
`kit/mkkit.py` (dev PC only) builds one folder: the 6 C# tools (same `dotnet build -c Release` as the steps) + fxrec_boot.exe
(same clang command, `sb.fxrec_cmd`) + kh2pick, the .NET 8 runtime only (from the SDK), OpenKH DLLs, busybox-w32 `sh.exe`,
python.org 3.12 embeddable + numpy/Pillow/oead, ffmpeg (gyan 9.0.2), vgmstream r2117. `kit.json` = sha256 of every file +
`sb.src_hash` of each tool's sources: sorabuild refuses a kit tool built from other sources.
Config `"kit": <dir>` -> the build steps copy the prebuilt tools; dotnet / openkh / bash / ffmpeg / vgmstream default to the kit's.
Run sorabuild with the kit's own `python/python.exe -E -P sorabuild.py ...`.

Config `"kh2img": <Steam KH2 Image/dt>` (instead of `"kh2"`) -> kh2pick extracts only the files in `kit/kh2_files.json`
(made by `kit/kh2list.py` from a last-access trace of a clean run: names + sizes + sha256, no game bytes) into `work/kh2`,
checking every sha256 (a modded or other KH2 build is refused with a plain message).
