# Legend of Kairi: source

The full source of [Legend of Kairi](../README.md), open source under the [MIT License](LICENSE) so anyone can learn
from it, fix it or build on it. It is a fork of [Savage Wind](https://github.com/Revivedx/SavageWindRecomp), the
Breath of the Wild PC recompilation (its own README: [README-SavageWind.md](README-SavageWind.md)).

**No game files are in here.** Everything taken from Breath of the Wild or Kingdom Hearts II is built on the player's
PC from their own copies of the games, by the installer's build tools below.

## What's where

| Path | What it is |
|---|---|
| `runtime/sora*.c`, `runtime/sora*.h` | The mod itself: `sora_mod.dll`, hot-reloadable while the game runs (Sora, Roxas, Mickey, Donald and Goofy, combos, magic, Drive Forms, summons, KH2 HUD and menus, multiplayer) |
| `runtime/sora_draw.c`, `sora_fx.c`, `sora_hud.c`, `sora_sfx.c` | The parts that live in the game exe: drawing KH2 models, effects, HUD and sounds in BotW's frame |
| `runtime/linkpos.c` (+ `linkpos_*.inc`) | The bridge into BotW's game code: Link's body, enemies, damage, AI hooks, the API table the DLL calls (`runtime/sw_api.h`) |
| `runtime/sora_net*.h` | Co-op multiplayer (UDP, up to 4 players): players, shared enemy health, enemy sync |
| `runtime/test/` | Host unit tests (plain C, no game needed) |
| `tools/sorabuild/` | The build tools that make every mod file from the player's own KH2 + BotW files (models, motions, effects, sounds, HUD, logos, skeletons), each output checked by sha256 (`verify.py`, `manifest.thispc.json`) |
| `tools/sorabuild/fxrec/` | Records KH2's own effects by running KH2's effect code on its own files |
| `tools/sorabuild/sounds/mine/` | Mines the sound timings (which sound plays on which frame) from KH2's files |
| `tools/installer/` | The friend installer (window, checks, `mkzip.py` release builder) |
| everything else | Savage Wind (the recompiler, runtime and its tools) |

## Building

You need your own Breath of the Wild 1.9.0 (Switch) dump and KINGDOM HEARTS HD 1.5+2.5 ReMIX (Steam).

1. **Toolchain (Windows):** [MSYS2](https://www.msys2.org/) UCRT64 with clang, make, SDL3, FFmpeg, Vulkan headers and
   shaderc. Set up Savage Wind's own build first: see [README-SavageWind.md](README-SavageWind.md) (it also covers Linux).
2. **Game + mod:** `make gen`, then `make -j` builds `build/savagewind.exe`, the game library and `build/sora_mod.dll`.
   `make sora` rebuilds only the mod DLL (the running game picks it up).
3. **Mod data:** Python 3.12 with numpy, Pillow and oead, the .NET 8 SDK (the C# tools under `tools/sorabuild/cs/`
   use [OpenKH](https://github.com/OpenKH/OpenKh)), ffmpeg and vgmstream (fetched automatically). Copy
   `tools/sorabuild/config.thispc.json`, point it at your KH2 extract / KH2 exe / BotW romfs / output folders, then
   `python tools/sorabuild/sorabuild.py --config <your config> all` and `python tools/sorabuild/verify.py --expect
   tools/sorabuild/manifest.thispc.json`. Expect ALL SAME: the build is deterministic.
4. **Tests:** each file in `runtime/test/` has its build line at the top (`clang -O1 -Iruntime runtime/test/<name>.c`).

Some paths in comments and defaults still say `D:/SoraBotW/...` (the dev PC's layout). The mod maps them to the
folder in the `SORA_ROOT` environment variable (`runtime/sora_path.h`), which the launchers set.

## License and credits

MIT (see [LICENSE](LICENSE)): Savage Wind's notice stays with it, plus tvllerr's for Legend of Kairi. Third-party
code in `third_party/` keeps its own license. Full credits are in the [main README](../README.md#credits).

Kingdom Hearts is © Disney / Square Enix; The Legend of Zelda and Breath of the Wild are © Nintendo. This is an
unofficial fan project.
