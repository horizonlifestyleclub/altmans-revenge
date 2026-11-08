"""Fresh, isolated skirmishes for Brutal Plus (reuses the Adaptive test harness).

  .venv/bin/python mod-src/launch_brutalplus_test.py
      play: you (Allies) vs Brutal Plus China on The Alamo
  .venv/bin/python mod-src/launch_brutalplus_test.py --versus
      watch: Brutal Plus vs Classic Brutal (the original AI) on a 3-player map.
      You are allied with both and only watch; your MCV deploys by itself.
  add --smoke-test --seconds 900 to run either one headless (no window).
  add --llm to let the local Qwen commander (llm-bridge/commander.py, needs
  Ollama running) steer the top Brutal Plus AI; --model picks the model.
  --enemy N / --classic N   country index (0 Americans, 4 British, 7 Confederation, 8 China, 9 Yuri)
Logs: brutalplus-playtest/test-game.log (or smoke-test.log); AI lines start
with [brutalplus] (Brutal Plus) and [singularity] AI ... (Classic Brutal).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

t = __import__('launch_adaptive_test')
t.PROFILE = 'Singularity - Brutal Plus'
t.TEST = t.REPO / 'brutalplus-playtest'
VERSUS_MAP = t.ROOT / 'work/singularity/maps/xmp09t3.map'   # 3 start points; replaces scenario 0 in the test copy only

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--smoke-test', action='store_true')
    parser.add_argument('--versus', action='store_true')
    parser.add_argument('--seconds', type=int, default=600)
    parser.add_argument('--enemy', type=int, default=8)
    parser.add_argument('--classic', type=int, default=8)
    parser.add_argument('--llm', action='store_true')
    parser.add_argument('--model', default='qwen3:4b')
    args = parser.parse_args()
    binary = t.prepare()
    mod = t.TEST / 'mods/yr' / t.PROFILE
    scene = mod / 'xmp29u2.map'
    if scene.exists() or scene.is_symlink():
        scene.unlink()
    ini = mod / 'RA2MD.INI'
    text = ini.read_text(encoding='cp1252')
    # Slot codes: 4 hardest AI (Brutal Plus), 5 medium (Brutal Plus Veteran), 6 easy slot (Classic Brutal).
    text = re.sub(r'(?m)^Slot01=.*$', 'Slot01=4,%d,-2' % args.enemy, text, count=1)
    if not args.versus and not args.smoke_test:   # a normal game: the usual 10 starting units
        text = re.sub(r'(?m)^UnitCount=.*$', 'UnitCount=10', text, count=1)
    if args.versus:
        shutil.copyfile(VERSUS_MAP, scene)
        text = re.sub(r'(?m)^Slot02=.*$', 'Slot02=6,%d,-2' % args.classic, text, count=1)
        os.environ['SG_BP_SPECTATE'] = '1'
        os.environ['SG_BP_AUTODEPLOY'] = '1'
    if args.smoke_test and not args.llm:   # with the commander, keep normal speed so it can keep up
        text = text.replace('GameSpeed=2', 'GameSpeed=0')
    if args.smoke_test:
        os.environ['SG_BP_AUTODEPLOY'] = '1'   # test only: deploy the idle player's MCV
    ini.write_text(text, encoding='cp1252')
    command = [str(binary), '--run', '--game', str(t.ROOT / 'outputs/Red-Alert-2-game-files'),
               '--mod', t.PROFILE, '--press', '0xE2:1667@1', '--press', '0x100:1401@2',
               '--press', '0x102:1559@3', '--waitlog', 'Capture_Mouse@4']
    if args.smoke_test:
        command += ['--headless', '--mute', '--watchdog', str(args.seconds)]
    if args.llm and not args.prepare_only:
        run = t.REPO / 'llm-bridge/run'
        run.mkdir(parents=True, exist_ok=True)
        for name in ('ledger.json', 'orders.json'):
            if (run / name).exists():
                (run / name).unlink()
        os.environ['SG_LLM_DIR'] = str(run)
        bridge_log = (run / 'commander.log').open('ab')
        subprocess.Popen([sys.executable, str(t.REPO / 'llm-bridge/commander.py'), '--model', args.model, '--dir', str(run),
                          '--game-pid', str(os.getpid())],   # execv below keeps this pid for the game
                         stdout=bridge_log, stderr=bridge_log, start_new_session=True)
        print('Qwen commander started (log: llm-bridge/run/commander.log).', flush=True)
    if args.prepare_only:
        print('Prepared isolated test:', t.TEST)
    else:
        print('Starting a fresh Brutal Plus %s. Normal game settings are preserved.'
              % ('vs Classic Brutal match' if args.versus else 'skirmish'), flush=True)
        log = t.TEST / ('smoke-test.log' if args.smoke_test else 'test-game.log')
        with log.open('wb') as handle:
            os.dup2(handle.fileno(), 1)
            os.dup2(handle.fileno(), 2)
        os.chdir(t.TEST)
        os.execv(str(binary), command)
