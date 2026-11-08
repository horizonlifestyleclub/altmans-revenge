"""v09 synthesized robot-dog sounds (no speech): servo chirps, motor whirs, an
electronic bark and a power-down. Writes WAVs + a manifest and packs them into a
mod's audio.bag/idx, replacing earlier copies of the same names.
Run: sfx_v09.py [--install MODDIR]"""
from pathlib import Path
import argparse
import json
import struct
import wave
import numpy as np

ROOT = __import__('sgpaths').ROOT
OUT = ROOT / 'outputs/singularity-build/v09/sfx'
RATE = 22050
rng = np.random.default_rng(9)


def t(sec):
    return np.arange(int(RATE * sec)) / RATE


def env(n, a=0.005, r=0.05):
    e = np.ones(n)
    ai, ri = int(a * RATE), int(r * RATE)
    if ai:
        e[:ai] = np.linspace(0, 1, ai)
    if ri:
        e[-ri:] *= np.linspace(1, 0, ri)
    return e


def lowpass(x, k):
    return np.convolve(x, np.ones(k) / k, mode='same')


def square(freq, x):
    return np.sign(np.sin(2 * np.pi * np.cumsum(freq) / RATE)) * 0.6 + np.sin(2 * np.pi * np.cumsum(freq) / RATE) * 0.4


def servo(sec, f0, f1):
    x = t(sec)
    f = np.linspace(f0, f1, len(x)) * (1 + 0.04 * np.sin(2 * np.pi * 37 * x))
    tone = square(f, x) * 0.35
    noise = lowpass(rng.standard_normal(len(x)), 6) * 0.25
    gear = (np.sin(2 * np.pi * 90 * x) > 0.6) * 0.15
    return (tone + noise * (0.5 + 0.5 * np.sin(2 * np.pi * 18 * x)) + gear) * env(len(x), 0.01, 0.06)


def beep(sec, f):
    x = t(sec)
    return np.sin(2 * np.pi * f * x) * 0.55 * env(len(x), 0.003, 0.03)


def bark(pitch=1.0, sec=0.22):
    x = t(sec)
    f0 = (300 - 160 * x / sec) * pitch
    carrier = square(f0, x)
    formant = np.sin(2 * np.pi * np.cumsum(f0 * 3.1) / RATE)
    y = carrier * (0.6 + 0.4 * formant) * np.sin(2 * np.pi * 55 * x + 1)   # ring-mod growl
    y = np.round(y * 6) / 6                                                   # coarse, mechanical
    y += lowpass(rng.standard_normal(len(x)), 3) * 0.2
    return y * env(len(x), 0.004, 0.08) * 0.9


def gap(sec):
    return np.zeros(int(RATE * sec))


def powerdown():
    x = t(1.1)
    f = 900 * np.exp(-3.2 * x) + 40
    y = square(f, x) * 0.45 * np.exp(-1.2 * x)
    crack = (rng.random(len(x)) > 0.985) * rng.standard_normal(len(x)) * 0.8
    return (y + crack * np.exp(-2 * x)) * env(len(x), 0.002, 0.2)


def resonate(x, f, q=8.0):
    """Two-pole band-pass resonator (one formant)."""
    w = 2 * np.pi * f / RATE
    r = np.exp(-w / (2 * q))
    a1, a2 = -2 * r * np.cos(w), r * r
    y = np.zeros_like(x)
    for i in range(2, len(x)):
        y[i] = x[i] - a1 * y[i - 1] - a2 * y[i - 2]
    return y * (1 - r)


def metal(x, ms=2.6, fb=0.45):
    """Short comb filter: a hollow, armoured body resonance."""
    d = int(RATE * ms / 1000)
    y = x.copy()
    for i in range(d, len(y)):
        y[i] += fb * y[i - d]
    return y


def voiced(sec, f0a, f0b, jitter=0.03):
    x = t(sec)
    f0 = np.linspace(f0a, f0b, len(x)) * (1 + jitter * lowpass(rng.standard_normal(len(x)), 200) * 8)
    ph = np.cumsum(f0) / RATE
    pulses = (np.diff(np.floor(ph), prepend=0) > 0).astype(float)
    return pulses * 2.0 + rng.standard_normal(len(x)) * 0.08


def growl(sec=0.55, f0=85):
    src = voiced(sec, f0 * 1.1, f0 * 0.9, 0.08)
    v = resonate(src, 420, 5) + 0.7 * resonate(src, 1150, 6) + 0.3 * resonate(src, 2400, 8)
    trem = 0.65 + 0.35 * np.sin(2 * np.pi * 23 * t(sec))
    return metal(v * trem) * env(len(v), 0.06, 0.12)


def bark2(sec=0.2, f0=260):
    src = voiced(sec, f0 * 1.25, f0 * 0.7, 0.02) + rng.standard_normal(int(RATE * sec)) * 0.4 * np.exp(-t(sec) * 30)
    v = resonate(src, 750, 4) + 0.8 * resonate(src, 1600, 5) + 0.35 * resonate(src, 2900, 7)
    e = np.exp(-t(sec) * 9) * env(int(RATE * sec), 0.004, 0.05)
    return metal(v * e, 1.9, 0.5)


def clank(sec=0.12, base=900):
    x = t(sec)
    y = sum(np.sin(2 * np.pi * base * k * x) * np.exp(-x * (40 + 25 * k)) for k in (1, 1.93, 2.94))
    return y * 0.5


def whir(sec, f0, f1):
    x = t(sec)
    f = np.linspace(f0, f1, len(x))
    hum = np.sin(2 * np.pi * np.cumsum(f) / RATE) + 0.4 * np.sin(4 * np.pi * np.cumsum(f) / RATE)
    grit = lowpass(rng.standard_normal(len(x)), 10) * 0.6
    return (hum * 0.25 + grit * 0.3) * env(len(x), 0.03, 0.08)


def mix(*parts):
    n = max(off + len(y) for off, y in parts)
    out = np.zeros(int(n))
    for off, y in parts:
        o = int(off)
        out[o:o + len(y)] += y
    return out


R = lambda sec: int(RATE * sec)
SOUNDS = {
    'SGDogSelect': [mix((0, whir(0.25, 140, 220)), (R(0.12), growl(0.45, 80))),
                    mix((0, growl(0.35, 95)), (R(0.3), whir(0.18, 160, 120))),
                    mix((0, whir(0.15, 120, 200)), (R(0.1), bark2(0.14, 230) * 0.6))],
    'SGDogMove': [mix((0, whir(0.4, 110, 190)), (R(0.08), clank()), (R(0.24), clank(0.1, 820))),
                  mix((0, clank(0.1, 950)), (R(0.12), clank(0.1, 870)), (R(0.05), whir(0.35, 130, 210)))],
    'SGDogAttack': [mix((0, growl(0.35, 90)), (R(0.3), bark2()), (R(0.55), bark2(0.18, 240))),
                    mix((0, bark2(0.22, 280)), (R(0.26), bark2(0.2, 250)), (R(0.5), growl(0.3, 75)))],
    'SGDogDie': [mix((0, powerdown() * 0.7), (R(0.05), bark2(0.25, 180) * 0.6), (R(0.5), clank(0.2, 600)), (R(0.62), clank(0.2, 520)))],
}


def pcm(y):
    y = y / max(1e-6, np.abs(y).max()) * 0.85
    return (y * 32767).astype('<i2').tobytes()


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest, samples = {}, []
    for group, clips in SOUNDS.items():
        manifest[group] = []
        for i, y in enumerate(clips):
            name = 'sgd%s%d' % (group[5:].lower()[:4], i)
            data = pcm(y)
            with wave.open(str(OUT / (name + '.wav')), 'wb') as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(RATE)
                w.writeframes(data)
            manifest[group].append(name)
            samples.append((name, data))
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return samples


def install(mod, samples):
    idx = (mod / 'audio.idx').read_bytes()
    magic, version, count = struct.unpack_from('<III', idx)
    names = {n for n, _ in samples}
    entries = [idx[12 + 36 * i:12 + 36 * (i + 1)] for i in range(count)]
    old = [e for e in entries if e[:16].split(b'\0')[0].decode() in names]
    entries = [e for e in entries if e[:16].split(b'\0')[0].decode() not in names]
    bag = bytearray((mod / 'audio.bag').read_bytes())
    # Reinstalling reuses the tail the previous copies occupied, so the bag does not grow.
    if old:
        start = min(struct.unpack_from('<I', e, 16)[0] for e in old)
        if all(struct.unpack_from('<I', e, 16)[0] < start for e in entries):
            del bag[start:]
    for name, data in samples:
        entries.append(struct.pack('<16s5I', name.encode(), len(bag), len(data), RATE, 4, 0))
        bag += data
    entries.sort(key=lambda b: b[:16].split(b'\0')[0].lower())
    (mod / 'audio.bag').write_bytes(bytes(bag))
    (mod / 'audio.idx').write_bytes(struct.pack('<III', magic, version, len(entries)) + b''.join(entries))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    a = ap.parse_args()
    samples = build()
    for mod in a.install:
        install(mod, samples)
    print('Robot dog sounds: %d clips in %s, installed %d' % (len(samples), OUT, len(a.install)))
