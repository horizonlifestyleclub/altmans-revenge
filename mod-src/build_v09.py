"""Rebuild build-macos/ra2-singularity after v09 changes (the plain ra2 build is untouched)."""
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = __import__('sgpaths').ROOT
repo = ROOT / 'outputs/redalert2-recomp'
build = repo / 'build-macos'
subprocess.run([sys.executable, str(ROOT / 'work/singularity/patch_v09_native.py')], check=True)
cmds = [__import__('sgpaths').fix_cmd(c) for c in (ROOT / 'work/singularity/build-commands.txt').read_text().splitlines()]
changed = sys.argv[1:] or ['/src/linux/main.c', '/src/recomp/gen/recomp_0005.c']
for name in changed:
    cmd = next(c for c in cmds if ' -c ' in c and c.rstrip().endswith(name))
    print('compiling', name, flush=True)
    subprocess.run(shlex.split(cmd), cwd=build, check=True)
link = shlex.split(cmds[-1].removeprefix(': && ').removesuffix(' && :'))
out = build / 'ra2-singularity.new'
link[link.index('-o') + 1] = str(out)
subprocess.run(link, cwd=build, check=True)
out.replace(build / 'ra2-singularity')
print('Built', build / 'ra2-singularity')
