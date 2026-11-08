"""v10 voices: LINK operator radio lines (added to the original Engineer voice)
and a short electronic chirp set for the operator drones themselves.
Lines: macOS voice (free, local), field-radio processing from voice_v09.
Packs into the mod audio via sfx_v09.install.
Run: voice_v10.py [--install MODDIR]"""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import tempfile
import wave
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
from sfx_v09 import install, RATE, beep, gap, env
from voice_v09 import radio

ROOT = __import__('sgpaths').ROOT
OUT = ROOT / 'outputs/singularity-build/v10/voice'
VOICE = 'Eddy (English (US))'
# New LINK lines, mixed with the Engineer's own clips in rules_v10.
LINES = {
    'SGLinkRadioSelect': ["Link established.", "Drone feed is up.", "Operator on the line."],
    'SGLinkRadioMove': ["Repositioning the link.", "Keeping the bird with me."],
    'SGLinkRadioAttack': ["Drone away.", "Target painted.", "Sending the bird in."],
    'SGLinkRadioLost': ["Link lost.", "Bird's down, spinning up another."],
}


def speak(text, tmp):
    aiff, wav = tmp / 'l.aiff', tmp / 'l.wav'
    subprocess.run(['/usr/bin/say', '-v', VOICE, '-r', '185', '-o', str(aiff), text], check=True)
    subprocess.run(['/usr/bin/afconvert', '-f', 'WAVE', '-d', 'LEI16@%d' % RATE, '-c', '1', str(aiff), str(wav)], check=True)
    with wave.open(str(wav), 'rb') as w:
        return np.frombuffer(w.readframes(w.getnframes()), '<i2')


def pcm(y):
    y = np.asarray(y, float)
    return (np.clip(y / max(1e-6, np.abs(y).max()) * 0.8, -1, 1) * 32767).astype('<i2').tobytes()


def chirps():
    """Drone acknowledgements: two- and three-tone chirps, no speech."""
    out = {}
    out['SGDroneSelect'] = [pcm(np.concatenate([beep(0.06, 1800), gap(0.03), beep(0.08, 2400)])),
                            pcm(np.concatenate([beep(0.05, 2100), gap(0.02), beep(0.05, 2100), gap(0.02), beep(0.07, 2700)]))]
    out['SGDroneMove'] = [pcm(np.concatenate([beep(0.07, 1500), gap(0.02), beep(0.07, 1900)])),
                          pcm(np.concatenate([beep(0.09, 1700), gap(0.03), beep(0.05, 1300)]))]
    out['SGDroneAttack'] = [pcm(np.concatenate([beep(0.05, 2600), gap(0.02), beep(0.05, 2600), gap(0.02), beep(0.12, 3200)]))]
    out['SGDroneDive'] = [pcm(np.concatenate([beep(0.04, 3000), gap(0.01), beep(0.04, 3000), gap(0.01), beep(0.04, 3000), gap(0.01), beep(0.25, 3600)]))]
    return out


def write(name, data):
    with wave.open(str(OUT / (name + '.wav')), 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest, samples, script = {}, [], []
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        for group, texts in LINES.items():
            manifest[group] = []
            for i, text in enumerate(texts):
                name = 'sgl%s%d' % (group[11:].lower()[:4], i)
                data = radio(speak(text, tmp))
                write(name, data)
                manifest[group].append(name)
                samples.append((name, data))
                script.append('- %s / %s: %s' % (group, name, text))
    for group, clips in chirps().items():
        manifest[group] = []
        for i, data in enumerate(clips):
            name = 'sgd%s%d' % (group[7:].lower()[:4], i)
            write(name, data)
            manifest[group].append(name)
            samples.append((name, data))
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (OUT / 'LINES.md').write_text('# LINK operator radio lines (v10)\n\nmacOS voice %s, field-radio processing; mixed with the original Engineer clips.\n\n' % VOICE + '\n'.join(script) + '\n')
    return samples


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    a = ap.parse_args()
    samples = build()
    for mod in a.install:
        install(mod, samples)
    print('v10 voices: %d clips, installed %d' % (len(samples), len(a.install)))
