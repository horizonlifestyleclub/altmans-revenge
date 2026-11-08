"""LLM Commander bridge for Singularity - Brutal Plus.

  python3 llm-bridge/commander.py [--model qwen3:4b] [--dir llm-bridge/run]

Watches <dir>/ledger.json (written by the game every 3 s when the game runs
with SG_LLM_DIR=<dir>), asks a local Ollama model for a stance, and writes
<dir>/orders.json. Never blocks the game: if the model is slow, it simply
skips that ledger, and the game falls back to its own rules after 60 s.

When ledgers stop arriving for 20 s after a match, it writes
memory/last-match.md, archives the match under memory/matches/, and asks the
model to rewrite memory/playbook.md (kept to about one page).
Only the Python standard library is used.
"""
import argparse
import json
import os
import shutil
import time
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
MEMORY = HERE / 'memory'
OLLAMA = os.environ.get('OLLAMA_HOST', 'http://127.0.0.1:11434')

STANCES = ['attack', 'defend', 'harass', 'expand']
TARGETS = ['core', 'factory', 'refinery', 'conyard', 'army']
BUILDS = ['armour', 'infantry', 'antiair', 'artillery']

SYSTEM = """You are the commander of one AI army in a Red Alert 2 skirmish (Singularity mod).
Your units already fight on their own: they gather into waves, attack together,
retreat when hurt, defend the base and miners. You only choose the strategy.
Every few seconds you get a report of what your side can SEE (enemy units out
of sight are unknown). Reply with ONLY one JSON object:
{"stance": one of attack|defend|harass|expand,
 "target": one of core|factory|refinery|conyard|army,
 "build": one of armour|infantry|antiair|artillery,
 "note": "under 12 words: why"}
Meaning: attack = launch waves sooner; defend = keep the army home;
harass = small early raids on enemy refineries/miners; expand = save money
and grow the economy, attack later. core = the enemy Singularity (it wins the
game if finished, so always stop it). antiair when enemy air or drones appear.
Change your mind only when the situation changes.

"""


def memory_text():
    core = (MEMORY / 'core-rules.md').read_text() if (MEMORY / 'core-rules.md').exists() else ''
    book = (MEMORY / 'playbook.md').read_text() if (MEMORY / 'playbook.md').exists() else ''
    return core + '\n' + book


def ollama(messages, timeout, num_predict=120, json_mode=True, think=False):
    body = {'model': ARGS.model, 'messages': messages, 'stream': False, 'think': think,
            'keep_alive': '30m', 'options': {'temperature': 0.3, 'num_predict': num_predict}}
    if json_mode:
        body['format'] = 'json'
    req = urllib.request.Request(OLLAMA + '/api/chat', json.dumps(body).encode(),
                                 {'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)['message']['content']


def top(d, n=8):
    items = sorted(d.items(), key=lambda kv: -kv[1])[:n]
    return ', '.join('%s %d' % kv for kv in items) or 'none'


def describe(L, held=(None, 0)):
    e = L['seen_enemy']
    lw = L['last_wave']
    wave = 'none yet'
    if L['waves']:
        wave = 'launched $%d' % lw['launched_value']
        if lw['end_value'] >= 0:
            wave += ', came back with $%d (%d survivors)' % (lw['end_value'], lw['survivors'])
        else:
            wave += ', still fighting'
    return '\n'.join([
        'Minute %d. You are %s. Credits $%d. Power %d/%d.' % (
            L['min'], L['side'], L['credits'], L['power'][0], L['power'][1]),
        'Your army: %d units worth $%d (%d waiting at home), %d miners. State: %s, %d in current wave.' % (
            L['army']['units'], L['army']['value'], L['army']['free'], L['army']['harvesters'],
            L['army_state'], L['wave_units']),
        'Your units: ' + top(L['my_units']),
        'Your buildings: ' + top(L['my_buildings'], 12),
        'Waves so far: %d; last wave: %s.' % (L['waves'], wave),
        'Enemy value attacking your base right now: $%d.' % L['base_threat'],
        'Enemy you can see: %d units worth $%d (%d near your base, %d near your ore, %d air, %d drones).' % (
            e['units'], e['value'], e['near_my_base'], e['near_my_ore'], e['air'], e['drones']),
        'Enemy unit types seen: ' + top(e['unit_types']),
        'Enemy buildings seen: ' + top(e['buildings'], 12),
        'Your current orders: %s / %s / %s.' % (
            L['current_orders']['stance'] or 'none', L['current_orders']['target'] or '-',
            L['current_orders']['build'] or '-'),
    ] + ['Hint: ' + x for x in hints(L, held)])


def hints(L, held):
    """Plain facts a small model tends to miss, worked out in Python."""
    h = []
    e = L['seen_enemy']
    if L['min'] >= 6 and L['army']['value'] >= 4000:
        h.append('Your army ($%d) is big enough to attack.' % L['army']['value'])
    if L['credits'] > 8000:
        h.append('You have $%d banked: money is not the problem, expanding is not needed.' % L['credits'])
    if e['units'] == 0 and not e['buildings']:
        h.append('You have not seen the enemy yet: attack or harass so your units find them.')
    if e['near_my_base'] or L['base_threat'] > 1500:
        h.append('Enemies are at your base: consider defend.')
    if e['air'] + e['drones'] > 3:
        h.append('Enemy air/drones seen: build antiair.')
    if any(k.startswith('SGCOLO') or k.startswith('SGNODE') for k in e['buildings']):
        h.append('The enemy is building a Singularity: attack with target core.')
    if held[0] and held[1] >= 4:
        h.append('You have kept stance "%s" for %d minutes; check it still fits.' % held)
    return h


def clean(raw):
    d = json.loads(raw)
    out = {'stance': str(d.get('stance', '')).lower(), 'target': str(d.get('target', '')).lower(),
           'build': str(d.get('build', '')).lower(), 'note': str(d.get('note', ''))[:90]}
    if out['stance'] not in STANCES:
        raise ValueError('bad stance %r' % out['stance'])
    if out['target'] not in TARGETS:
        out['target'] = 'factory'
    if out['build'] not in BUILDS:
        out['build'] = 'armour'
    out['note'] = out['note'].replace('"', "'").replace('\\', ' ')
    return out


def write_atomic(path, text):
    tmp = path.with_suffix(path.suffix + '.tmp')
    tmp.write_text(text)
    os.replace(tmp, path)


def end_of_match(run, decisions):
    log = run / 'match.jsonl'
    if not log.exists():
        return
    rows = []
    for x in log.read_text(encoding='utf-8', errors='replace').splitlines():
        try:
            rows.append(json.loads(x))
        except ValueError:
            pass  # a half-written or damaged line
    if len(rows) < 100:   # under ~5 minutes: nothing worth learning from
        print('[commander] match too short to learn from', flush=True)
        return
    last = rows[-1]
    peak = max(rows, key=lambda r: r['army']['value'])
    stamp = time.strftime('%Y-%m-%d_%H%M')
    lines = ['# Last match (%s)' % stamp, '',
             '- Lasted %d game minutes as %s.' % (last['min'], last['side']),
             '- Peak army $%d at minute %d; final army $%d, %d buildings.' % (
                 peak['army']['value'], peak['min'], last['army']['value'], sum(last['my_buildings'].values())),
             '- Waves launched: %d.' % last['waves'],
             '- At the end it saw %d enemy units ($%d) and %d enemy buildings.' % (
                 last['seen_enemy']['units'], last['seen_enemy']['value'],
                 sum(last['seen_enemy']['buildings'].values())),
             '- Final buildings: ' + top(last['my_buildings'], 20), '',
             '## Orders given (when they changed)']
    prev = None
    for d in decisions:
        key = (d['stance'], d['target'], d['build'])
        if key != prev:
            lines.append('- min %d: %s / %s / %s - %s' % (d['min'], *key, d['note']))
            prev = key
    lines.append('')
    lines.append('## Situation every 5 minutes')
    for r in rows:
        if r['frame'] % 4500 < 45:
            lines.append('- min %d: army $%d, credits $%d, enemy seen $%d, base threat $%d, waves %d' % (
                r['min'], r['army']['value'], r['credits'], r['seen_enemy']['value'], r['base_threat'], r['waves']))
    text = '\n'.join(lines) + '\n'
    MEMORY.mkdir(exist_ok=True)
    (MEMORY / 'last-match.md').write_text(text)
    arch = MEMORY / 'matches' / stamp
    arch.mkdir(parents=True, exist_ok=True)
    (arch / 'summary.md').write_text(text)
    shutil.copyfile(log, arch / 'match.jsonl')
    (arch / 'decisions.json').write_text(json.dumps(decisions, indent=0))
    print('[commander] match over after %d minutes; summary in %s' % (last['min'], arch), flush=True)
    playbook = (MEMORY / 'playbook.md').read_text() if (MEMORY / 'playbook.md').exists() else ''
    core = (MEMORY / 'core-rules.md').read_text() if (MEMORY / 'core-rules.md').exists() else ''
    try:
        raw = ollama([
            {'role': 'system', 'content': 'You keep a short list of LESSONS for an RTS AI commander. '
             'It already has fixed core rules (shown for reference; do not repeat them). '
             'Rewrite the lessons using the new match report: keep lessons that still hold, fix ones the '
             'match disproved, add at most 3 new ones. Only write what this match actually showed. Each rule is one concrete sentence under 25 words '
             '(e.g. "If enemy drones > 3, build antiair."). Use only these words for orders: stances '
             'attack/defend/harass/expand, targets core/factory/refinery/conyard/army, builds '
             'armour/infantry/antiair/artillery. "core" means the enemy Singularity building. '
             'Reply ONLY with JSON: {"rules": ["...", "..."]} with 1 to 8 lessons.'},
            {'role': 'user', 'content': 'Core rules (fixed):\n' + core + '\nCurrent lessons:\n' + playbook + '\n\nMatch report:\n' + text}],
            timeout=180, num_predict=600)
        rules = [str(r).strip() for r in json.loads(raw).get('rules', []) if 10 < len(str(r).strip()) < 220]
        if 1 <= len(rules) <= 8:
            if playbook:
                (arch / 'playbook-before.md').write_text(playbook)
            (MEMORY / 'playbook.md').write_text('# Commander playbook (lessons the model learned; rewritten after every match)\n\n' + ''.join('- %s\n' % r for r in rules))
            print('[commander] playbook rewritten (%d rules)' % len(rules), flush=True)
        else:
            print('[commander] playbook kept: model returned %d usable rules' % len(rules), flush=True)
    except Exception as exc:
        print('[commander] playbook rewrite failed: %s' % exc, flush=True)


def game_alive():
    if not ARGS.game_pid:
        return True
    try:
        os.kill(ARGS.game_pid, 0)
        return True
    except OSError:
        return False


def main():
    run = Path(ARGS.dir).resolve()
    run.mkdir(parents=True, exist_ok=True)
    ledger = run / 'ledger.json'
    print('[commander] model %s, watching %s' % (ARGS.model, ledger), flush=True)
    try:  # warm the model up so the first real answer is fast
        ollama([{'role': 'user', 'content': 'Reply {"ok":1}'}], timeout=60, num_predict=8)
    except Exception as exc:
        print('[commander] cannot reach Ollama at %s: %s' % (OLLAMA, exc), flush=True)
    seen = 0.0
    seq = 0
    decisions = []
    history = []
    last_frame = -1
    while True:
        time.sleep(0.25)
        try:
            mtime = ledger.stat().st_mtime
        except FileNotFoundError:
            if not game_alive():
                return
            continue
        if mtime == seen:
            if decisions and (time.time() - mtime > ARGS.match_gap or not game_alive()):
                end_of_match(run, decisions)
                decisions, history = [], []
            if not decisions and not game_alive():
                print('[commander] game closed; stopping', flush=True)
                return
            continue
        seen = mtime
        try:
            L = json.loads(ledger.read_text(encoding='utf-8', errors='replace'))
        except Exception:
            continue
        if L['frame'] < last_frame:  # a new match started without a gap
            if decisions:
                end_of_match(run, decisions)
            decisions, history = [], []
        last_frame = L['frame']
        if L['frame'] % ARGS.every_frames >= 45 and decisions:
            continue  # ask every ~ARGS.every_frames frames, not on every ledger
        held = (None, 0)  # current stance and how many minutes it has been held
        if decisions:
            i = len(decisions) - 1
            while i > 0 and decisions[i - 1]['stance'] == decisions[-1]['stance']:
                i -= 1
            held = (decisions[-1]['stance'], L['min'] - decisions[i]['min'])
        report = describe(L, held)
        messages = [{'role': 'system', 'content': SYSTEM + memory_text()}, {'role': 'user', 'content': report}]
        t0 = time.time()
        try:
            raw = ollama(messages, timeout=ARGS.timeout)
            order = clean(raw)
            # Safety rule: after minute 10 with money banked, "expand" means attack.
            if order['stance'] == 'expand' and L['min'] >= 10 and L['credits'] > 5000:
                order['stance'] = 'attack'
                order['note'] = '(expand overruled: money banked) ' + order['note']
        except Exception as exc:
            print('[commander] min %d: no order (%s); the game keeps playing on its own' % (L['min'], exc), flush=True)
            continue
        took = time.time() - t0
        seq += 1
        order['seq'] = seq
        write_atomic(run / 'orders.json', json.dumps(order))
        decisions.append(dict(order, min=L['min'], frame=L['frame'], seconds=round(took, 2)))
        history += [{'role': 'user', 'content': report}, {'role': 'assistant', 'content': raw}]
        with (run / 'decisions.jsonl').open('a') as fh:
            fh.write(json.dumps(decisions[-1]) + '\n')
        print('[commander] min %d (%.1fs): %s / %s / %s - %s' % (
            L['min'], took, order['stance'], order['target'], order['build'], order['note']), flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--model', default=os.environ.get('SG_LLM_MODEL', 'qwen3:4b'))
    p.add_argument('--dir', default=str(HERE / 'run'))
    p.add_argument('--timeout', type=float, default=3.0, help='seconds to wait for each answer')
    p.add_argument('--every-frames', type=int, default=150, help='ask every N game frames (150 = 10 s)')
    p.add_argument('--game-pid', type=int, default=0, help='stop when this process ends')
    p.add_argument('--match-gap', type=float, default=20.0, help='seconds without ledgers = match over')
    ARGS = p.parse_args()
    main()
