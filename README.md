# Legend of Kairi

Play Breath of the Wild as Sora (with Roxas and Mickey) from Kingdom Hearts II, on the Savage Wind PC recompilation.
Includes Drive Forms, Keyblades, magic, summons, the KH2 command menu and HUD, and co-op multiplayer for up to 4 players.

**No game files are included.** The installer builds every mod file on your PC from your own copies of the games
(about 15 minutes). Your games stay as they are.

## What you need

- Windows 10 / 11 (64-bit), about 15 GB free disk space
- **Savage Wind 0.4.1-alpha.4** (this version only), already set up with your own BotW game files: start it once first
- **KINGDOM HEARTS HD 1.5+2.5 ReMIX** on Steam, **without KH2 mods installed** (the installer checks every KH2 file it reads;
  if you have a KH2 mod in the .pkg files, restore the originals or use Steam's "Verify integrity of game files")

## Install

1. Download `Legend-of-Kairi-0.1.0-alpha.zip` from the [Releases page](https://github.com/tvllerr/legend-of-kairi/releases/latest) and unzip it into an empty folder (not inside the Savage Wind folder).
2. Double-click `Install Legend of Kairi.cmd`.
3. Click **Choose...** next to Savage Wind folder and pick the `Savage Wind` folder (the one with `extracted` inside).
   KINGDOM HEARTS is found automatically from Steam.
4. Click **Install** and wait for the finish screen (about 15 minutes).
5. Play with `Play Legend of Kairi.bat`.

## Multiplayer (co-op)

- Every player needs this exact same release.
- Players connect directly (UDP port 7790). Easiest: everyone installs [Tailscale](https://tailscale.com) and joins the same tailnet.
- Right-click `Play Legend of Kairi Multiplayer.bat` > Edit, replace `FILL-IN-FRIENDS-TAILSCALE-IP` on the `set PEERS` line with your
  friend's Tailscale IP (several friends: separate them with commas), save, then start it.
- Synced: players, Sora's forms and effects, skateboards, ragdolls, shared enemy health and deaths, enemies moving and attacking the same.

## Known limits (0.1.0-alpha)

- Co-op is new: enemy attacks copied from a friend's game may land a little late or miss now and then.
- Only one copy of the game can run at a time on one PC.

## Download check

`Legend-of-Kairi-0.1.0-alpha.zip` sha256: `6e83635029f021203e1a5438a8346fd65aade21d7743333743c3551a1f8a9cd9`

## If something goes wrong

The install window shows what stopped and has **Open logs**: send the `logs` folder to tvllerr.

## Credits and licenses

Built on Savage Wind (MIT, Revivedx and contributors). Uses OpenKH (Apache 2.0), Python, .NET, Pillow, numpy, vgmstream, FFmpeg,
SDL3 and WebView2; full license texts are in the `LICENSES` folder of the zip.
Kingdom Hearts and The Legend of Zelda belong to their owners; this is a free fan mod and contains none of their files.
