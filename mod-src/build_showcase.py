"""Build an isolated showcase entry point, sharing unchanged native engine objects."""
from pathlib import Path
import json
import shlex
import subprocess

ROOT = __import__('sgpaths').ROOT
repo = ROOT / 'outputs/redalert2-recomp'
work = ROOT / 'work/singularity/showcase-runtime'
work.mkdir(exist_ok=True)
scene = json.loads((repo / 'unit-showcase/inventory.json').read_text())
vectors = {'Structures': '0xA83C68', 'Units': '0xA83CE0', 'Infantry': '0xA8E348', 'Aircraft': '0xA8B218'}
rows = []
for o in scene['objects']:
    owner = 0 if o['owner'] == '<Player @ A>' else 1 if o['owner'] == '<Player @ B>' else 2
    rows.append('{"%s",%s,%d,%d,%d,%d}' % (o['unit'], vectors[o['kind']], o['x'], o['y'], owner, o['rank']))
fixture = (ROOT / 'work/singularity/showcase_runtime.h').read_text().replace('/* INVENTORY */', ',\n'.join(rows))
(work / 'fixture.h').write_text(fixture)
header = (repo / 'src/linux/singularity.h').read_text()
header = header.replace('void sg_tick(void) {', '#include "fixture.h"\nvoid sg_tick(void) {')
header = header.replace('    sg_lastframe=f;', '    sg_lastframe=f;\n    sg_showcase_tick(f);')
(work / 'singularity_showcase.h').write_text(header)
main = (repo / 'src/linux/main.c').read_text().replace('#include "singularity.h"', '#include "singularity_showcase.h"')
(work / 'main-showcase.c').write_text(main)
cmds = [__import__('sgpaths').fix_cmd(c) for c in (ROOT / 'work/singularity/build-commands.txt').read_text().splitlines()]
cmd = shlex.split(next(c for c in cmds if ' -c ' in c and '/src/linux/main.c' in c))
obj = str(work / 'main-showcase.o')
cmd[cmd.index('-o') + 1] = obj
cmd[cmd.index('-MF') + 1] = obj + '.d'
cmd[cmd.index('-c') + 1] = str(work / 'main-showcase.c')
subprocess.run(cmd, cwd=repo / 'build-macos', check=True)
link = shlex.split(cmds[-1].removeprefix(': && ').removesuffix(' && :'))
link = [obj if a == 'CMakeFiles/ra2.dir/src/linux/main.c.o' else a for a in link]
link[link.index('-o') + 1] = str(repo / 'unit-showcase/build/ra2-showcase')
subprocess.run(link, cwd=repo / 'build-macos', check=True)
print('Private showcase executable built; normal game executable unchanged.')
