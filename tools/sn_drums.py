#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Snapper's drum kit: thirteen one-shots synthesized here, written as 16-bit mono WAVs at 44.1 kHz to
assets/samples-cc0/DRUMS/. No recording and no third-party sample goes into them: every sound is made by the code
below, and the WAVs are dedicated to the public domain (CC0 1.0, see assets/samples-cc0/ATTRIBUTION.txt).

    python3 tools/sn_drums.py            writes the WAVs (needs numpy)

The kit is the big 80s one: the snare, the clap and the three toms carry their gated room in the sample itself (a
dense synthetic room, loud, held flat and then cut dead: GATE_S), so the FM-1 plays it without a reverb of its own
running and it always sounds the same. Every hit starts at full level on its first sample (no fade-in). The files'
order is their key on the FM-1 (F3 upward), as tools/gen_samples.py maps a kit: 00 KICK, 01 KICK ROUND, 02 SNARE,
03 CLAP, 04 HAT, 05 OPEN HAT, 06 CRASH, 07 TOM HI, 08 TOM MID, 09 TOM LO, and the drum machine's 10 BOX KICK, 11 BOX
RIM, 12 BOX HAT (firmware/src/sn_sounds.h).
Run it again and the same bytes come out (the noise is seeded).
"""
import struct
from pathlib import Path

import numpy as np

SR = 44100
OUT = Path(__file__).resolve().parents[1] / "assets" / "samples-cc0" / "DRUMS"
rng = np.random.default_rng(0x5A9E)


def t_axis(sec):
    return np.arange(int(sec * SR)) / SR


def noise(n):
    return rng.uniform(-1.0, 1.0, n)


def onepole_hp(x, fc):
    """a one-pole high-pass at fc (Hz)"""
    a = np.exp(-2 * np.pi * fc / SR)
    y = np.zeros_like(x)
    prev_x = prev_y = 0.0
    for i, v in enumerate(x):
        prev_y = a * (prev_y + v - prev_x)
        prev_x = v
        y[i] = prev_y
    return y


def biquad(x, kind, fc, q):
    """RBJ cookbook biquad: 'lp', 'hp' or 'bp' (constant peak gain)"""
    w = 2 * np.pi * fc / SR
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    if kind == "lp":
        b = [(1 - c) / 2, 1 - c, (1 - c) / 2]
    elif kind == "hp":
        b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    else:
        b = [al, 0.0, -al]
    a = [1 + al, -2 * c, 1 - al]
    b = [v / a[0] for v in b]
    a = [1.0, a[1] / a[0], a[2] / a[0]]
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = b[0] * v + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
        x2, x1, y2, y1 = x1, v, y1, o
        y[i] = o
    return y


def env_exp(t, tau):
    return np.exp(-t / tau)


def norm(x, peak=0.98):
    return x * (peak / max(1e-9, np.max(np.abs(x))))


def fade_tail(x, sec):
    n = min(len(x), int(sec * SR))
    x[-n:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, n))
    return x


def sine_sweep(t, f0, f1, tau):
    """a sine whose pitch falls from f0 to f1 with time constant tau; starts at phase 0, so the body itself does
    not click: the beater (kick's tick) is the transient, shaped on its own"""
    f = f1 + (f0 - f1) * np.exp(-t / tau)
    ph = 2 * np.pi * np.cumsum(f) / SR
    return np.sin(ph - ph[0])


def kick(f0, f1, tau, decay, click, sec, drive):
    t = t_axis(sec)
    body = sine_sweep(t, f0, f1, tau) * env_exp(t, decay)
    # the beater: a 1.6 ms burst of high-passed noise and a 3 kHz tick, both gone at once
    n = noise(len(t))
    tick = onepole_hp(n, 2500) * env_exp(t, 0.0016) * click
    tick += np.sin(2 * np.pi * 3000 * t) * env_exp(t, 0.0009) * click * 0.6
    x = np.tanh((body + tick) * drive) / np.tanh(drive)
    return fade_tail(x, 0.04)


# the metal: six square waves at inharmonic ratios (the classic analogue cymbal recipe), through a band-pass
METAL_HZ = (205.3, 304.4, 369.6, 522.7, 540.0, 800.0)


def metal(t, scale=1.0):
    x = np.zeros_like(t)
    for f in METAL_HZ:
        x += np.sign(np.sin(2 * np.pi * f * scale * t + f))
    return x / len(METAL_HZ)


def hat(decay, sec, bright):
    t = t_axis(sec)
    m = metal(t, 1.0)
    m = biquad(m, "bp", 10000, 0.9) * 1.6 + biquad(m, "hp", 7600, 0.7)
    air = onepole_hp(noise(len(t)), 9000) * 0.45 * bright
    x = (m + air) * env_exp(t, decay)
    x += onepole_hp(noise(len(t)), 12000) * env_exp(t, 0.0012) * 0.8     # the tip of the stick
    x = biquad(x, "hp", 6000, 0.7)
    return fade_tail(x, min(0.04, sec / 4))


def crash():
    t = t_axis(1.0)
    m = metal(t, 1.37) + metal(t, 0.81) * 0.7
    m = biquad(m, "hp", 4200, 0.6)
    air = onepole_hp(noise(len(t)), 5000) * 0.8
    x = (m + air) * (env_exp(t, 0.42) * 0.85 + env_exp(t, 0.006) * 0.6)
    x = biquad(x, "hp", 3200, 0.7)
    return fade_tail(x, 0.35)


# ---- the gated room: a dense synthetic room (a burst of filtered noise decaying slowly), mixed in loud, then a gate
# that holds it flat for its time and shuts it in a few milliseconds. The snare's gate is the longest of the kit's
GATE_S = {"snare": 0.30, "clap": 0.24, "tom": 0.36}


def room_ir(sec, decay, bright):
    t = t_axis(sec)
    n = noise(len(t))
    n = biquad(n, "lp", 3200 + 4000 * bright, 0.7)
    n = onepole_hp(n, 220)
    ir = n * np.exp(-t / decay)
    ir[: int(0.006 * SR)] *= np.linspace(0.0, 1.0, int(0.006 * SR))   # (the room's first reflections, not the hit)
    return ir / max(1e-9, np.sqrt(np.sum(ir * ir)))


def convolve(x, h):
    n = len(x) + len(h) - 1
    m = 1 << (n - 1).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, m) * np.fft.rfft(h, m), m)[:n]
    return y


def gated(dry, gate, wet=1.6, bright=0.6):
    """the dry hit with its room, held and cut: full for gate seconds (the room swells into a flat block of
    sound), then down in 12 ms to silence"""
    room = convolve(dry, room_ir(0.9, 0.55, bright))
    room = room[: len(dry)] if len(room) > len(dry) else room
    x = dry[: len(room)] + wet * norm(room, 1.0) * np.max(np.abs(dry))
    t = t_axis(len(x) / SR)
    g = np.ones_like(t)
    rel = 0.012
    k = t > gate
    g[k] = np.clip(1.0 - (t[k] - gate) / rel, 0.0, 1.0)
    x = np.tanh(x * g * 1.1) / np.tanh(1.1)          # the hit slammed into the room: a squash, as a desk would
    end = int((gate + rel + 0.002) * SR)
    return x[:end]


def gated_snare():
    t = t_axis(0.42)
    tone = (np.sin(2 * np.pi * 180 * t) * 0.9 + np.sin(2 * np.pi * 330 * t) * 0.45) * env_exp(t, 0.07)
    wires = biquad(noise(len(t)), "bp", 3600, 0.6) * 2.4
    wires = onepole_hp(wires, 1200) * env_exp(t, 0.11)
    snap = onepole_hp(noise(len(t)), 5000) * env_exp(t, 0.004) * 1.5
    dry = np.tanh((tone * 0.85 + wires + snap) * 1.7)
    return gated(dry, GATE_S["snare"], 0.75, 0.75)


def gated_clap():
    t = t_axis(0.36)
    n = biquad(noise(len(t)), "bp", 1300, 1.3) + biquad(noise(len(t)), "bp", 2700, 1.0) * 0.6
    e = np.zeros_like(t)
    for k, at in enumerate((0.0, 0.010, 0.0195, 0.028)):   # four hands, ~9 ms apart
        d = t - at
        e += np.where(d >= 0, np.exp(-np.clip(d, 0, None) / 0.0045), 0.0) * (1.0 - 0.1 * k)
    dry = onepole_hp(np.tanh(n * e * 2.6), 450)
    return gated(dry, GATE_S["clap"], 0.6, 0.6)


def tom(f0, f1):
    """a tom: a deep sine falling a little in pitch, a stick on its head, and the gated room"""
    t = t_axis(0.55)
    body = sine_sweep(t, f0, f1, 0.09) * env_exp(t, 0.22)
    body += sine_sweep(t, f0 * 1.52, f1 * 1.52, 0.07) * env_exp(t, 0.09) * 0.25   # (the head's second mode)
    stick = onepole_hp(noise(len(t)), 2200) * env_exp(t, 0.003) * 0.8
    dry = np.tanh((body + stick) * 1.8)
    return gated(dry, GATE_S["tom"], 0.55, 0.45)


# ---- the drum machine: the small, dry kit of a preset rhythm box, for the hush and the rise
def m_kick():
    """a short low thump: a sine at 62 Hz, a quick fall in pitch, gone in a fifth of a second"""
    t = t_axis(0.22)
    x = sine_sweep(t, 120, 60, 0.012) * env_exp(t, 0.06)
    return fade_tail(np.tanh(x * 1.3), 0.03)


def m_rim():
    """the box's rim: two resonant tones a fifth and a bit apart, a click, 40 ms"""
    t = t_axis(0.06)
    x = (np.sin(2 * np.pi * 1700 * t) + 0.6 * np.sin(2 * np.pi * 2600 * t)) * env_exp(t, 0.008)
    x += onepole_hp(noise(len(t)), 3000) * env_exp(t, 0.0015) * 0.6
    return fade_tail(x, 0.01)


def m_hat():
    """the box's hat: a breath of metal, 40 ms"""
    t = t_axis(0.05)
    m = biquad(metal(t, 1.6), "hp", 7000, 0.7)
    x = (m + onepole_hp(noise(len(t)), 8000) * 0.5) * env_exp(t, 0.011)
    return fade_tail(x, 0.01)


KIT = [
    ("00_kick", lambda: kick(150, 44, 0.050, 0.42, 0.9, 0.70, 2.4)),
    ("01_kick_round", lambda: kick(110, 38, 0.075, 0.70, 0.45, 0.90, 1.6)),
    ("02_snare", gated_snare),
    ("03_clap", gated_clap),
    ("04_hat", lambda: hat(0.030, 0.10, 0.9)),
    ("05_open_hat", lambda: hat(0.24, 0.52, 1.0)),
    ("06_crash", crash),
    ("07_tom_hi", lambda: tom(205, 160)),
    ("08_tom_mid", lambda: tom(150, 116)),
    ("09_tom_lo", lambda: tom(108, 80)),
    ("10_box_kick", m_kick),
    ("11_box_rim", m_rim),
    ("12_box_hat", m_hat),
]


def write_wav(path, x):
    data = np.clip(np.round(norm(x) * 32767), -32768, 32767).astype("<i2").tobytes()
    fmt = struct.pack("<HHIIHH", 1, 1, SR, SR * 2, 2, 16)
    body = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(body)) + b"WAVE" + body)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*.wav"):
        old.unlink()
    for name, fn in KIT:
        x = fn()
        write_wav(OUT / f"{name}.wav", x)
        print(f"sn_drums: {name}.wav  {len(x) / SR * 1000:.0f} ms")


if __name__ == "__main__":
    main()
