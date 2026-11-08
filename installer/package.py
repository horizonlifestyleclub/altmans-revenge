"""Package Altman's Revenge as one zip for Windows and macOS players.

  python3 installer/package.py            # -> dist/AltmansRevenge-<version>.zip

Build the mod first (mod-src/build_mod.py, then mod-src/build_brutalplus.py).
The zip holds the two mod folders (symlinks resolved to real files), the Qwen
commander without anyone's match history, and the installers. It holds no
game executable and no files of the retail game beyond what the mod itself
overrides, so players use their own copy of the game.
"""
from pathlib import Path
import os
import shutil
import sys
import zipfile

REPO = Path(__file__).resolve().parent.parent
VERSION = (sys.argv[1] if len(sys.argv) > 1 else 'v11')
MODS = ['Singularity', 'Singularity - Brutal Plus']
# ra2md.ini: the player's own settings live there. audio.bag/audio.idx: the
# game's whole retail sound bank with the v09 effects merged in - not ours to
# redistribute; without them the game uses the player's own bank (those few
# new effects are silent; voices are loose .wav files and still play).
SKIP_MOD = {'ra2md.ini', 'audio.bag', 'audio.idx'}
BRIDGE = ['commander.py', 'master.py', 'README.md', 'memory/core-rules.md', 'memory/strategies/v001.md']

out = REPO / 'dist' / ('AltmansRevenge-' + VERSION)
if out.exists():
    shutil.rmtree(out)
payload = out / 'payload'
for name in MODS:
    src = REPO / 'mods/yr' / name
    if not (src / 'rulesmd.ini').exists():
        raise SystemExit('Build the mod first: %s has no rulesmd.ini' % src)
    dst = payload / 'mods/yr' / name
    dst.mkdir(parents=True)
    for f in sorted(src.iterdir()):
        if f.name.lower() in SKIP_MOD or f.name.startswith('.') or not f.resolve().is_file():
            continue
        shutil.copyfile(f.resolve(), dst / f.name)    # follows the shared-art symlinks
for rel in BRIDGE:
    (payload / 'llm-bridge' / rel).parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(REPO / 'llm-bridge' / rel, payload / 'llm-bridge' / rel)
# Sounds: only the mod's own clips; the installer merges them into the
# player's own sound bank (installer/merge_audio.py).
sys.path.insert(0, str(REPO / 'installer'))
import merge_audio
game = next((g for g in [Path(os.environ.get('RA2_GAME_DIR', '/nonexistent')), REPO / 'game'] + [p / 'game' for p in REPO.parents]
             if (g / 'langmd.mix').exists() or (g / 'audio.idx').exists()), None)
if not game:
    raise SystemExit('Set RA2_GAME_DIR to a Red Alert 2 game folder (for the original sound bank)')
orig_idx, orig_bag = merge_audio.find_bank(game)
tmp = out / '.orig'
tmp.mkdir()
(tmp / 'audio.idx').write_bytes(orig_idx)
(tmp / 'audio.bag').write_bytes(orig_bag)
(payload / 'sounds').mkdir()
clips, size = merge_audio.make_patch(tmp / 'audio.idx', REPO / 'mods/yr/Singularity/audio.idx', payload / 'sounds/patch.idx')
shutil.rmtree(tmp)
print('sounds: %d mod clips (%.1f MB)' % (clips, size / 1e6))
shutil.copyfile(REPO / 'installer/merge_audio.py', payload / 'merge_audio.py')
(payload / 'VERSION').write_text(VERSION + '\n')
for f in (REPO / 'installer/mac').iterdir():
    shutil.copy2(f, out / f.name)
for f in (REPO / 'installer/windows').iterdir():
    shutil.copy2(f, out / f.name)
shutil.copyfile(REPO / 'installer/README.md', out / 'README.md')

archive = out.with_suffix('.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for f in sorted(out.rglob('*')):
        if f.is_file():
            info = zipfile.ZipInfo.from_file(f, Path(out.name) / f.relative_to(out))
            if f.suffix == '.command':
                info.external_attr = 0o100755 << 16     # stays double-clickable on macOS
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, f.read_bytes())
size = archive.stat().st_size / 1e6
print('Packaged %s (%.1f MB)' % (archive, size))
