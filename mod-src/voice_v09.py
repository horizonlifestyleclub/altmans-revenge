"""v09 BASTION crew voice: original Ukrainian lines, macOS Ukrainian voice (Lesya),
processed as a field radio (band-limited, light drive, squelch click, static).
Free and local; no paid service. Packs into the mod audio via sfx_v09.install.
Run: voice_v09.py [--install MODDIR]"""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import tempfile
import wave
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
from sfx_v09 import install, RATE

ROOT = __import__('sgpaths').ROOT
OUT = ROOT / 'outputs/singularity-build/v09/voice'
LINES = {
    'SGBastionSelect': ["Бастіон на зв'язку.", "Рій готовий.", "Слухаю, командире.", "Котушки повні, пташки чекають."],
    'SGBastionMove': ["Змінюємо позицію.", "Рушаємо.", "Прийняв, виїжджаємо.", "Тихо і швидко."],
    'SGBastionAttack': ["Пташки в повітрі!", "Запускаю FPV.", "Ціль бачу, працюю.", "Кабель тягнеться, ціль наша."],
}
rng = np.random.default_rng(3)


def radio(y):
    x = y.astype(float) / 32768
    # band-limit roughly 350-3200 Hz with simple one-pole filters
    hp = np.zeros_like(x)
    a = np.exp(-2 * np.pi * 350 / RATE)
    for i in range(1, len(x)):
        hp[i] = a * (hp[i - 1] + x[i] - x[i - 1])
    lp = np.zeros_like(hp)
    b = np.exp(-2 * np.pi * 3200 / RATE)
    for i in range(1, len(hp)):
        lp[i] = (1 - b) * hp[i] + b * lp[i - 1]
    v = np.tanh(lp * 3.2) * 0.7                              # light overdrive
    v += rng.standard_normal(len(v)) * 0.012                 # static bed
    click = np.zeros(int(RATE * 0.04))
    click[:30] = rng.standard_normal(30) * 0.5               # squelch click in and out
    v = np.concatenate([click, np.zeros(int(RATE * 0.03)), v, np.zeros(int(RATE * 0.05)), click * 0.6])
    return (np.clip(v / max(1e-6, np.abs(v).max()) * 0.9, -1, 1) * 32767).astype('<i2').tobytes()


def speak(text, tmp):
    aiff, wav = tmp / 'l.aiff', tmp / 'l.wav'
    subprocess.run(['/usr/bin/say', '-v', 'Lesya', '-r', '168', '-o', str(aiff), text], check=True)
    subprocess.run(['/usr/bin/afconvert', '-f', 'WAVE', '-d', 'LEI16@%d' % RATE, '-c', '1', str(aiff), str(wav)], check=True)
    with wave.open(str(wav), 'rb') as w:
        return np.frombuffer(w.readframes(w.getnframes()), '<i2')


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest, samples, script = {}, [], []
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        for group, texts in LINES.items():
            manifest[group] = []
            for i, text in enumerate(texts):
                name = 'sgb%s%d' % (group[9:].lower()[:4], i)
                data = radio(speak(text, tmp))
                with wave.open(str(OUT / (name + '.wav')), 'wb') as w:
                    w.setnchannels(1)
                    w.setsampwidth(2)
                    w.setframerate(RATE)
                    w.writeframes(data)
                manifest[group].append(name)
                samples.append((name, data))
                script.append('- %s / %s: %s' % (group, name, text))
    (OUT / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    (OUT / 'LINES.md').write_text('# BASTION crew lines (Ukrainian, original)\n\nmacOS voice Lesya, field-radio processing.\n\n' + '\n'.join(script) + '\n')
    return samples


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    a = ap.parse_args()
    samples = build()
    for mod in a.install:
        install(mod, samples)
    print('Bastion voice: %d lines, installed %d' % (len(samples), len(a.install)))
