"""Path roots for the Singularity mod scripts, per checkout.

The scripts were written for one fixed layout:
    <workspace>/outputs/redalert2-recomp   (this repo)
    <workspace>/work/singularity           (scripts + extracted game data)
To let several git worktrees build side by side, every script now uses
ROOT = <this checkout>/.sgroot, a small folder of links that recreates that
layout but points `outputs/redalert2-recomp` at THIS checkout and
`work/singularity/<script>` at THIS checkout's mod-src. Extracted game data
(never committed) is shared read-only from the real work/singularity.
Lab folders (lab-*) are real per-checkout folders, so labs never collide.
"""
from pathlib import Path
import os

HERE = Path(__file__).resolve().parent          # <checkout>/mod-src
REPO = HERE.parent                              # <checkout>


def _find_workspace():
    env = os.environ.get('SG_WORKSPACE')
    if env:
        return Path(env)
    for p in REPO.parents:
        if (p / 'work/singularity/patch/rulesmd.ini').exists():
            return p
    raise SystemExit('Cannot find workspace with work/singularity data; set SG_WORKSPACE')


WORKSPACE = _find_workspace()
DATA = WORKSPACE / 'work/singularity'            # shared extracted data
MAIN_REPO = WORKSPACE / 'outputs/redalert2-recomp'
ROOT = REPO / '.sgroot'


def _link(dst, src):
    if dst.is_symlink():
        if Path(os.readlink(dst)) == src:
            return
        dst.unlink()
    elif dst.exists():
        return
    dst.symlink_to(src)


def _build_root():
    outputs = ROOT / 'outputs'
    work = ROOT / 'work/singularity'
    outputs.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)
    _link(outputs / 'redalert2-recomp', REPO)
    for p in (WORKSPACE / 'outputs').iterdir():
        if p.name != 'redalert2-recomp' and not p.name.startswith('.'):
            _link(outputs / p.name, p)
    scripts = {p.name for p in HERE.iterdir() if p.is_file()}
    for p in DATA.iterdir():
        if p.name in scripts or p.name.startswith('.'):
            continue
        if p.is_dir() and p.name.startswith('lab'):
            (work / p.name).mkdir(exist_ok=True)   # per-checkout lab output
            continue
        _link(work / p.name, p)
    for name in scripts:
        _link(work / name, HERE / name)


def fix_cmd(cmd):
    """build-commands.txt was recorded in the main repo; retarget a command line."""
    cmd = cmd.replace('@WORKSPACE@', str(WORKSPACE))   # stored without anyone's home folder
    return cmd.replace(str(MAIN_REPO), str(REPO))


_build_root()
