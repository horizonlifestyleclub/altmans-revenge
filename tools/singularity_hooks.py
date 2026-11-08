"""Put the Singularity mod's native hooks into a fresh Yuri's Revenge lift.

  python3 tools/singularity_hooks.py [src/recomp/gen]

The hooks are small calls added at the start of a few lifted game functions
(prices, build speed, drone weapons, AI production...). Each one does nothing
unless the Singularity mod is the active mod, so the plain game plays as it
shipped. Running it again changes nothing (every hook carries a marker).
build-macos.sh and build-linux.sh run it before building Yuri's Revenge, so
every native build can play Altman's Revenge without a special build.
"""
from pathlib import Path
import os
import sys
import tempfile
import types

REPO = Path(__file__).resolve().parent.parent
gen = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else REPO / 'src/recomp/gen'
if not (gen / 'recomp_dispatch.c').exists():
    raise SystemExit('No lift in %s: run setup first' % gen)
if not (gen / 'recomp_0046.c').exists() or 'RECOMP_ENTER(0x006F3330u);' not in (gen / 'recomp_0046.c').read_text():
    print('Not a Yuri\'s Revenge (gamemd) lift: no Singularity hooks needed')
    raise SystemExit(0)

# The patch scripts find the lift through sgpaths.ROOT/outputs/redalert2-recomp;
# point that at a small folder whose src/recomp/gen is this lift.
with tempfile.TemporaryDirectory() as tmp:
    fake = Path(tmp) / 'outputs/redalert2-recomp'
    (fake / 'src/recomp').mkdir(parents=True)
    os.symlink(gen, fake / 'src/recomp/gen')
    sys.modules['sgpaths'] = types.SimpleNamespace(ROOT=Path(tmp))
    for name in ['patch_native', 'patch_v06_native', 'patch_v07_native', 'patch_v09_native', 'patch_v10_native']:
        script = REPO / 'mod-src' / (name + '.py')
        exec(compile(script.read_text(), str(script), 'exec'), {'__name__': '__main__', '__file__': str(script)})
print('Singularity hooks in place:', gen)
