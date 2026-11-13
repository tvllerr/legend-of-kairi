#!/usr/bin/env python3
"""(s216 kbfx) Our own Keyblade sounds, SYNTHESISED here (numpy only: no KH2 / BotW sample goes in). Writes the four
48 kHz mono PCM16 WAVs next to this script; the fx chain step fxsfx_kbown (fx/kbownfx.py) packs them into sfx.bin.

  python -E -P kbsynth.py [--out <dir>]

  kbs1_hit       Sealing Blade hit (s217 remake): a sword cut (bite + short thump + bright hiss) with a soft shimmer of
                 high partials and a quiet 2637 Hz chime on top. No low bar tone (the s216 one rang like a cowbell).
  kbg1_hit       Guardian Core hit: a low electric "thrum-zap" (a buzzing 78 Hz saw with a 31 Hz flutter + a falling saw zap
                 + crackle) with a short rising whine (900 -> 2500 Hz, snapped off at 0.16 s) and a dull stone knock under it.
  kbs1_beam      Sealing Blade beam launch: an airy rising slash (band noise sweeping 700 -> 5500 Hz) + a faint rising chime.
  kbs1_beam_hit  where the beam lands: a bright impact (click + high bar modes + chime) with a short low thump for weight.

Loudness: each one is scaled so its loudest 100 ms has the RMS of KH2's own Keyblade hit in our sfx.bin ("hit", 0.078),
peak kept under 0.6. Deterministic (fixed seeds): a re-run writes the same bytes.
"""
import argparse
import os
import wave

import numpy as np

SR = 48000
TARGET_LOUD = 0.08   # RMS of the loudest 100 ms (KH2 se500 #0 in sfx.bin: 0.078)
PEAK_MAX = 0.6


def t_axis(sec):
    return np.arange(int(SR * sec)) / SR


def env_exp(t, tau, attack=0.002):
    e = np.exp(-t / tau)
    if attack > 0:
        e *= np.clip(t / attack, 0, 1)
    return e


def svf_bandpass(x, fc, q):
    """state-variable band-pass, fc may be an array (a sweep)"""
    fc = np.broadcast_to(np.asarray(fc, np.float64), x.shape)
    y = np.zeros_like(x)
    lp = bp = 0.0
    damp = 1.0 / q
    for i in range(len(x)):
        f = 2.0 * np.sin(np.pi * min(fc[i], SR * 0.22) / SR)
        hp = x[i] - lp - damp * bp
        bp += f * hp
        lp += f * bp
        y[i] = bp
    return y


def onepole_lp(x, fc):
    a = np.exp(-2 * np.pi * fc / SR)
    y = np.zeros_like(x)
    s = 0.0
    for i in range(len(x)):
        s = (1 - a) * x[i] + a * s
        y[i] = s
    return y


def sweep_phase(f):
    return 2 * np.pi * np.cumsum(f) / SR


def saw(phase):
    return 2 * ((phase / (2 * np.pi)) % 1.0) - 1


def bar_modes(t, f0, taus, amps, detune=1.2):
    """a struck free bar: modes 1 : 2.756 : 5.404 : 8.933 (inharmonic = metallic), each a slightly beating pair"""
    out = np.zeros_like(t)
    for k, (r, tau, a) in enumerate(zip((1.0, 2.756, 5.404, 8.933), taus, amps)):
        f = f0 * r
        out += a * env_exp(t, tau, 0.0008) * (np.sin(2 * np.pi * f * t + k) + 0.6 * np.sin(2 * np.pi * (f + detune * (k + 1)) * t))
    return out


def fade_tail(x, sec=0.03):
    n = min(len(x), int(SR * sec))
    x[-n:] *= np.linspace(1, 0, n)
    return x


def normalise(x):
    w = int(SR * 0.1)
    loud = max(float(np.sqrt(np.mean(x[k:k + w] ** 2))) for k in range(0, max(1, len(x) - w), w // 4))
    x = x * (TARGET_LOUD / loud)
    pk = float(np.abs(x).max())
    if pk > PEAK_MAX:
        x *= PEAK_MAX / pk
    return x


# ------------------------------------------------------------------------------------------------- the sounds
def sealing_hit(ring=1.0, chime_amp=0.14):
    """(s217: the s216 version rang like a cowbell, the user's words: its loudest part was one struck-bar tone at 1180 Hz.
    Now a sword cut: the bite + a short thump for weight, a bright hiss, and a soft shimmer of many HIGH partials with no
    single loud note; the holy chime stays, much quieter.)"""
    rng = np.random.default_rng(2171)
    t = t_axis(1.0)
    # the bite: noise, its band falling 3.2k -> 900 Hz in about 40 ms
    cut = svf_bandpass(rng.standard_normal(len(t)), 900 + 2300 * np.exp(-t / 0.018), 1.3) * env_exp(t, 0.045, 0.001) * 1.3
    # weight: a 150 -> 75 Hz thump
    thump = np.sin(sweep_phase(75 + 75 * np.exp(-t / 0.03))) * env_exp(t, 0.055, 0.002) * 0.9
    # "sh": bright hiss, 8k -> 5k
    sh = svf_bandpass(rng.standard_normal(len(t)), 5000 + 3000 * np.exp(-t / 0.05), 2.0) * env_exp(t, 0.07, 0.002) * 0.9
    # "ing": a blade's shimmer: six high partials (not a bar's ratios), each a slowly beating pair, none dominant
    shim = np.zeros_like(t)
    for k, (f, tau, a) in enumerate(((3340, 0.30, 0.10), (4480, 0.24, 0.09), (5290, 0.20, 0.08), (6710, 0.15, 0.06),
                                     (7930, 0.11, 0.05), (9460, 0.08, 0.03))):
        shim += a * env_exp(t, tau, 0.003) * (np.sin(2 * np.pi * f * t + k) + 0.7 * np.sin(2 * np.pi * (f + 7 + 3 * k) * t))
    # the chime on top (E7 2637 Hz + a faint fifth), soft attack, quiet
    chime = env_exp(t, 0.35, 0.012) * (chime_amp * np.sin(2 * np.pi * 2637 * t) + 0.35 * chime_amp * np.sin(2 * np.pi * 3951 * t + 1.0))
    x = cut + thump + sh + ring * shim + chime
    return fade_tail(x)


def guardian_hit():
    rng = np.random.default_rng(2162)
    t = t_axis(0.8)
    n = rng.standard_normal(len(t))
    # thrum: a 78 Hz saw buzz, fluttering at 31 Hz, low-passed, 0.22 s
    thr = saw(2 * np.pi * 78 * t) * (0.65 + 0.35 * np.sin(2 * np.pi * 31 * t)) * env_exp(t, 0.2, 0.004)
    thr = onepole_lp(onepole_lp(thr, 700), 700) * 2.0  # (12 dB / octave: a dull buzz, not a rasp)
    # zap: a saw falling 2200 -> 260 Hz in ~60 ms, a little driven, + crackle
    fz = 260 + 1940 * np.exp(-t / 0.022)
    zap = np.tanh(2.2 * saw(sweep_phase(fz))) * env_exp(t, 0.05, 0.001) * 0.45
    crackle = (rng.random(len(t)) < 0.012) * rng.standard_normal(len(t)) * env_exp(t, 0.08, 0.0) * 0.9
    crackle = svf_bandpass(crackle, 3000, 1.5)
    # the rising whine, snapped off: 900 -> 2500 Hz over 0.16 s, a 9 Hz wobble, then cut in 8 ms
    tw = 0.16
    fw = np.where(t < tw, 900 + 1600 * (t / tw) ** 1.4, 2500) + 25 * np.sin(2 * np.pi * 9 * t)
    gate = np.clip((tw + 0.008 - t) / 0.008, 0, 1) * np.clip(t / 0.01, 0, 1)
    whine = (np.sin(sweep_phase(fw)) + 0.25 * np.sin(2 * sweep_phase(fw))) * gate * 0.28
    # the dull stone knock under it: a 150 -> 95 Hz thud + a low-passed click
    fk = 95 + 55 * np.exp(-t / 0.012)
    knock = np.sin(sweep_phase(fk)) * env_exp(t, 0.045, 0.0005) * 0.9
    knock += onepole_lp(n * env_exp(t, 0.006, 0.0), 1200) * 1.4
    x = thr + zap + crackle + whine + knock
    return fade_tail(x)


def sealing_beam():
    rng = np.random.default_rng(2163)
    t = t_axis(0.55)
    n = rng.standard_normal(len(t))
    # airy rising slash: band noise 700 -> 5500 Hz, swelling in 70 ms then falling away
    fc = 700 + 4800 * np.clip(t / 0.22, 0, 1) ** 0.8
    swell = np.clip(t / 0.07, 0, 1) ** 1.5 * np.exp(-np.clip(t - 0.07, 0, None) / 0.12)
    air = svf_bandpass(n, fc, 1.6) * swell * 1.2
    low = onepole_lp(n, 500) * swell * 0.8   # the body of the whoosh
    # a faint rising chime (1300 -> 2700 Hz)
    fch = 1300 + 1400 * np.clip(t / 0.25, 0, 1)
    chime = np.sin(sweep_phase(fch)) * swell * 0.16
    x = air + low + chime
    return fade_tail(x, 0.05)


def sealing_beam_hit():
    rng = np.random.default_rng(2164)
    t = t_axis(1.0)
    n = rng.standard_normal(len(t))
    click = svf_bandpass(n, 6000, 1.2) * env_exp(t, 0.012, 0.0005) * 1.3
    burst = svf_bandpass(n, 3200 + 2500 * np.exp(-t / 0.05), 1.4) * env_exp(t, 0.09, 0.001) * 0.7
    ring = bar_modes(t, 1560, (0.3, 0.16, 0.08, 0.04), (0.5, 0.4, 0.22, 0.1))
    chime = env_exp(t, 0.5, 0.004) * (0.3 * np.sin(2 * np.pi * 3136 * t) + 0.1 * np.sin(2 * np.pi * 4699 * t))
    thump = np.sin(sweep_phase(70 + 60 * np.exp(-t / 0.02))) * env_exp(t, 0.07, 0.001) * 0.8
    x = click + burst + ring + chime + thump
    return fade_tail(x)


SOUNDS = {"kbs1_hit": sealing_hit, "kbg1_hit": guardian_hit, "kbs1_beam": sealing_beam, "kbs1_beam_hit": sealing_beam_hit}


def write_wav(path, x):
    pcm = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
    tmp = path + ".tmp"
    with wave.open(tmp, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    os.replace(tmp, path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.dirname(os.path.abspath(__file__)))
    a = ap.parse_args()
    for name, fn in SOUNDS.items():
        x = normalise(fn())
        p = os.path.join(a.out, name + ".wav")
        write_wav(p, x)
        print("%-14s %.3f s, peak %.3f -> %s" % (name, len(x) / SR, float(np.abs(x).max()), p))


if __name__ == "__main__":
    main()
