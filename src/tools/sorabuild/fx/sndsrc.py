"""sorabuild: where the sfx tools find their sound inputs (the "sounds" switch).

  SB_SNDROOT (set by sorabuild in recipe mode) = work/sounds, a tree laid out like the old PHH cache but built by
  tools/sorabuild/sounds/build_sounds.py from work/sounds_recipe.json (step sounds_mine) + the player's own KH2 SCD files:
      mined/kh2_sounds.kh.json   bdx/motions.kh.json   kh2_sfx/<se|voice>/.../NNN.wav   kh2_hud.json
  Without SB_SNDROOT the tools read the PHH cache (SB_FXCACHE / SB_PHH) exactly as before ("sounds": "phh").
  SB_WAVTRACE=<file>: every wav a tool asks for is appended there (sounds/mine/mine_sounds.py uses this to find the used set).
Tools import this by putting their own dir on sys.path first.
"""
import os


def root():
    return os.environ.get("SB_SNDROOT") or (os.environ["SB_FXCACHE"] if "SB_FXCACHE" in os.environ else os.environ["SB_PHH"])


def sounds_json():
    return os.path.join(root(), "mined", "kh2_sounds.kh.json")


def motions_json():
    return os.path.join(root(), "bdx", "motions.kh.json")


def _trace(rel):
    t = os.environ.get("SB_WAVTRACE")
    if t:
        with open(t, "a", encoding="utf-8") as f:
            f.write(rel.replace("\\", "/") + "\n")


def wavrel(rel):
    """path of a wav given relative to kh2_sfx (e.g. 'se/se000/100.wav')"""
    _trace(rel)
    return os.path.join(root(), "kh2_sfx", rel)


def wavfile(s):
    """path of a sounds[key] entry's wav; entries with their own 'dir' (our extra wavs) are not traced"""
    if s.get("dir"):
        return os.path.join(s["dir"], s["file"])
    return wavrel(s["file"])
