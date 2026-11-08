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
MEMORY = Path(os.environ.get('SG_MEMORY_DIR', HERE / 'memory'))
OLLAMA = os.environ.get('OLLAMA_HOST', 'http://127.0.0.1:11434')

STANCES = ['attack', 'defend', 'harass', 'expand', 'retreat']
TARGETS = ['core', 'factory', 'refinery', 'conyard', 'army', 'defences']
BUILDS = ['armour', 'infantry', 'antiair', 'artillery']
NAVY = ['off', 'build', 'defend', 'attack']

SYSTEM = """You command one army in a Red Alert 2 skirmish, playing to win like a strong
"Brutal" AI. Your units fight on their own (waves, focus fire, retreat when hurt,
base and miner defence); you set the strategy. You only know what your side has SEEN:
what is in sight now, your memory of enemy bases and armies, and scout reports.
Follow your current strategy (below), adapt it to what you see, and say why.

Reply with ONLY one JSON object:
{"stance": "attack|defend|harass|expand|retreat",
 "target": "core|factory|refinery|conyard|army|defences" or an enemy building ID you have seen,
 "build": "armour|infantry|antiair|artillery" or a unit ID from "You can build",
 "scout": "now" or "",
 "raid": 0-8 units to send at enemy miners/refineries (0 = none),
 "navy": "off|build|defend|attack",
 "note": "under 12 words, why"}
attack = launch waves sooner; defend = army stays home; harass = raids, army waits;
expand = save money, attack later; retreat = call the wave home now.
core = the enemy Singularity: if it exists, destroy it first.
navy (only with a shipyard): build = shipyard makes warships that guard home; defend = same,
they fight anything near the shipyard; attack = fleet (4+ ships) sails at enemy ships, shipyards,
then coastal buildings. The navy runs alongside the land army; it does not replace it.

"""


def memory_text():
    """Fixed core rules + the master agent's current strategy + what we know about the opponent.
    Read fresh every turn, so a new strategy is used the moment the master writes it."""
    parts = []
    for name in ('core-rules.md', 'strategy.md', 'opponent.md'):
        if (MEMORY / name).exists():
            parts.append((MEMORY / name).read_text())
    return '\n'.join(parts)


def ollama(messages, timeout, num_predict=120, json_mode=True, think=False):
    body = {'model': ARGS.model, 'messages': messages, 'stream': False, 'think': think,
            'keep_alive': '30m', 'options': {'temperature': 0.3, 'num_predict': num_predict, 'num_ctx': 4096}}
    if json_mode:
        body['format'] = 'json'
    req = urllib.request.Request(OLLAMA + '/api/chat', json.dumps(body).encode(),
                                 {'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)['message']['content']


def top(d, n=8):
    items = sorted(d.items(), key=lambda kv: -kv[1])[:n]
    return ', '.join('%s %d' % kv for kv in items) or 'none'


def money(v):
    return '$%s' % format(int(v), ',')


def describe(L, events, held=(None, 0)):
    s = L['in_sight']
    k = L['enemy_army_known']
    w = L['wave']
    lm = L['last_minute']
    out = ['Minute %d. You are %s. Credits %s. Power %d/%d.' % (
        L['min'], L['side'], money(L['credits']), L['power'][0], L['power'][1])]
    if events:
        out.append('JUST HAPPENED: ' + '; '.join(events[-8:]) + '.')
    out.append('Your army: %d units worth %s (%d free at home), %d miners. Units: %s.' % (
        L['army']['units'], money(L['army']['value']), L['army']['free'], L['army']['harvesters'], top(L['my_units'])))
    out.append('Your buildings: ' + top(L['my_buildings'], 14) + '.')
    out.append('Last minute: you lost %d units (%s), killed %d enemy (%s). Enemy value attacking your base now: %s.' % (
        lm['lost_units'], money(lm['lost_value']), lm['killed_units'], money(lm['killed_value']), money(L['base_threat'])))
    if w['state'] != 'gather':
        out.append('Your wave #%d is %s: %d units %s left of %s launched, %s cells from target %s, %d fighting; since launch killed %s, lost %s.' % (
            w['number'], 'marching' if w['state'] == 'march' else 'assaulting', w['units'], money(w['value']),
            money(w['launched_value']), w['cells_to_target'], w['target'] or '?', w['fighting'],
            money(w['killed_since_launch']), money(w['lost_since_launch'])))
    else:
        lw = L['last_wave']
        out.append('Main army is gathering at home.' + (
            ' Last wave: launched %s, came back with %s.' % (money(lw['launched_value']), money(max(lw['end_value'], 0))) if w['number'] else ' No wave sent yet.'))
    sc = L['scout']
    out.append('Scout: ' + ('%s, %s.' % (sc['unit'], sc['where']) if sc['status'] == 'travelling' else
                            'lost recently.' if sc['status'] == 'lost' else 'none out.'))
    if L['raid']['units']:
        out.append('Raid: %d units attacking enemy %s.' % (L['raid']['units'], L['raid']['target']))
    if s['units'] or s['buildings']:
        out.append('IN SIGHT NOW: %d enemy units (%s; %d near your base, %d near your ore, %d air/drones): %s. Enemy buildings in sight: %s.' % (
            s['units'], money(s['value']), s['near_my_base'], s['near_my_ore'], s['air_or_drones'], top(s['unit_types']), top(s['buildings'])))
    else:
        out.append('IN SIGHT NOW: no enemy.')
    if k['units']:
        out.append('Enemy army you remember (seen in the last minute): %d units %s (%d air/drones), last seen %ds ago %s %d cells from your base, %s: %s.' % (
            k['units'], money(k['value']), k['air_or_drones'], k['seconds_since_seen'], k['direction'], k['cells_from_our_base'], k['heading'], top(k['types'])))
    else:
        out.append('Enemy army: not seen in the last minute.')
    for b in L['enemy_bases']:
        if not b['scouted'] and not b['buildings']:
            out.append('Enemy %d start area %s, %d cells away: NOT scouted yet, contents unknown.' % (b['house'], b['direction'], b['cells_from_our_base']))
            continue
        tech = [t for t, f in [('air', b['has_air']), ('SINGULARITY', b['has_singularity']), ('drones', b['has_drones']), ('navy', b['has_navy'])] if f]
        out.append('Enemy %d base %s, %d cells away (last seen %s): %d defences%s; buildings: %s.' % (
            b['house'], b['direction'], b['cells_from_our_base'],
            '%d min ago' % b['minutes_since_seen'] if b['minutes_since_seen'] >= 0 else 'never',
            b['defences'], ', has ' + ', '.join(tech) if tech else '', top(b['buildings'], 12)))
    nv = L.get('navy')
    if nv:
        if not nv['have_shipyard'] and not nv['ships']:
            out.append('Navy: you have no shipyard (no water here, or not built yet).')
        else:
            out.append('Navy (%s): %d warships worth %s, %s%s. Enemy ships remembered: %d (%s)%s; enemy shipyards known: %d.' % (
                nv['order'], nv['ships'], money(nv['value']), nv['state'], ' at ' + nv['target'] if nv['target'] else '',
                nv['enemy_ships_known'], money(nv['enemy_ships_value']), ': ' + top(nv['enemy_ship_types']) if nv['enemy_ship_types'] else '', nv['enemy_shipyards_known']))
    out.append('You can build: ' + ', '.join(L['can_build']) + '.')
    o = L['current_orders']
    out.append('Your current orders: %s / %s / %s, raid %d, navy %s.' % (o['stance'] or 'none', o['target'] or '-', o['build'] or '-', o['raid'], o.get('navy') or 'off'))
    out += ['Hint: ' + x for x in hints(L, held)]
    return '\n'.join(out)


def hints(L, held):
    """Plain facts a small model tends to miss, worked out in Python."""
    h = []
    k = L['enemy_army_known']
    bases = L['enemy_bases']
    if any(b['has_singularity'] for b in bases) or any(x.startswith(('SGCOLO', 'SGNODE')) for x in L['in_sight']['buildings']):
        h.append('The enemy has a Singularity: attack with target core NOW.')
    if any(not b['scouted'] and not b['buildings'] for b in bases) and L['scout']['status'] != 'travelling':
        h.append('An enemy start area is unscouted: scout now.')
    if L['min'] >= 6 and L['army']['value'] >= 5000 and L['wave']['state'] == 'gather' and L['base_threat'] < 1500:
        h.append('Your army (%s) is at home and big enough to attack.' % money(L['army']['value']))
    if L['credits'] > 8000:
        h.append('You have %s banked: money is not the problem, do not expand.' % money(L['credits']))
    if L['base_threat'] > 1500 or L['in_sight']['near_my_base'] >= 4:
        h.append('Enemies are at your base: defend.')
    if k['heading'] == 'toward our base' and k['value'] > L['army']['value'] * 0.6:
        h.append('A big enemy army is heading toward your base.')
    w = L['wave']
    if w['state'] != 'gather' and w['lost_since_launch'] > 2 * max(w['killed_since_launch'], 500) and w['value'] < w['launched_value'] * 0.5:
        h.append('Your wave is losing badly: consider retreat.')
    if k['air_or_drones'] + L['in_sight']['air_or_drones'] > 3:
        h.append('Enemy air/drones seen: build antiair.')
    if any(b['defences'] >= 5 for b in bases):
        h.append('An enemy base has many defences: artillery helps.')
    if L['min'] <= 8 and L['raid']['units'] == 0 and any(b['buildings'] for b in bases):
        h.append('Early game and you know an enemy base: a small raid (3-5) on its miners slows it.')
    nv = L.get('navy') or {}
    if nv.get('have_shipyard'):
        if nv.get('order', 'off') == 'off':
            h.append('You have a shipyard: set navy "build" so it makes warships.')
        if nv.get('enemy_ships_known', 0) > nv.get('ships', 0) and nv.get('ships', 0) < 4:
            h.append('Enemy ships outnumber yours: navy "defend" and build more.')
        if nv.get('ships', 0) >= 4 and nv.get('state') == 'home' and (nv.get('enemy_shipyards_known') or nv.get('enemy_ships_known')):
            h.append('Your fleet (%d ships) is ready and you know enemy naval targets: navy "attack".' % nv['ships'])
    if held[0] and held[1] >= 4:
        h.append('You have kept stance "%s" for %d minutes; check it still fits.' % held)
    return h


def clean(raw, L):
    d = json.loads(raw)
    out = {'stance': str(d.get('stance', '')).lower(), 'target': str(d.get('target', '')),
           'build': str(d.get('build', '')), 'scout': 'now' if str(d.get('scout', '')).lower() in ('now', 'yes', 'true', '1') else '',
           'note': str(d.get('note', ''))[:90]}
    try:
        out['raid'] = max(0, min(8, int(d.get('raid', 0) or 0)))
    except (TypeError, ValueError):
        out['raid'] = 0
    if out['stance'] not in STANCES:
        raise ValueError('bad stance %r' % out['stance'])
    known = {x for b in L['enemy_bases'] for x in b['buildings']} | set(L['in_sight']['buildings'])
    if out['target'].lower() in TARGETS:
        out['target'] = out['target'].lower()
    elif out['target'].upper() in known:
        out['target'] = out['target'].upper()
    else:
        out['target'] = 'factory'
    if out['build'].lower() in BUILDS:
        out['build'] = out['build'].lower()
    elif out['build'].upper() in L['can_build']:
        out['build'] = out['build'].upper()
    else:
        out['build'] = 'armour'
    nv = str(d.get('navy', '')).lower()
    have = (L.get('navy') or {}).get('have_shipyard')
    out['navy'] = nv if nv in NAVY else ('build' if have else 'off')
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
    if not rows or rows[-1]['min'] < 5:   # under 5 minutes: nothing worth learning from
        print('[commander] match too short to learn from', flush=True)
        return
    last = rows[-1]
    peak = max(rows, key=lambda r: r['army']['value'])
    stamp = time.strftime('%Y-%m-%d_%H%M')
    lines = ['# Last match (%s)' % stamp, '',
             '- Lasted %d game minutes as %s.' % (last['min'], last['side']),
             '- Peak army $%d at minute %d; final army $%d, %d buildings.' % (
                 peak['army']['value'], peak['min'], last['army']['value'], sum(last['my_buildings'].values())),
             '- Waves launched: %d.' % last['wave']['number'],
             '- Enemy bases known at the end: ' + '; '.join('%s %d cells, %d buildings%s' % (
                 b['direction'], b['cells_from_our_base'], sum(b['buildings'].values()), ', has Singularity' if b['has_singularity'] else '')
                 for b in last['enemy_bases']) + '.',
             '- Events: ' + '; '.join(e for r in rows for e in r.get('events', []))[:1500],
             '- Final buildings: ' + top(last['my_buildings'], 20), '',
             '## Orders given (when they changed)']
    prev = None
    for d in decisions:
        key = (d['stance'], d['target'], d['build'])
        if key != prev:
            lines.append('- min %d: %s / %s / %s, raid %d%s%s - %s' % (d['min'], *key, d.get('raid', 0), ', scout' if d.get('scout') else '', ', navy ' + d['navy'] if d.get('navy', 'off') != 'off' else '', d['note']))
            prev = key
    lines.append('')
    lines.append('## Situation every 5 minutes')
    for r in rows:
        if r['frame'] % 4500 < 45:
            lines.append('- min %d: army $%d, credits $%d, enemy army known $%d, base threat $%d, waves %d, lost/killed last minute $%d/$%d' % (
                r['min'], r['army']['value'], r['credits'], r['enemy_army_known']['value'], r['base_threat'], r['wave']['number'],
                r['last_minute']['lost_value'], r['last_minute']['killed_value']))
    text = '\n'.join(lines) + '\n'
    MEMORY.mkdir(exist_ok=True)
    (MEMORY / 'last-match.md').write_text(text)
    arch = MEMORY / 'matches' / stamp
    arch.mkdir(parents=True, exist_ok=True)
    (arch / 'summary.md').write_text(text)
    shutil.copyfile(log, arch / 'match.jsonl')
    (arch / 'decisions.json').write_text(json.dumps(decisions, indent=0))
    print('[commander] match over after %d minutes; summary in %s' % (last['min'], arch), flush=True)
    old = sorted(p for p in (MEMORY / 'matches').iterdir() if p.is_dir())
    for p in old[:-3]:   # keep only the last 3 matches; the lessons live in strategy.md / opponent.md
        shutil.rmtree(p, ignore_errors=True)
    if (run / 'score.jsonl').exists():
        shutil.copyfile(run / 'score.jsonl', arch / 'score.jsonl')
    # The master agent reviews the match and rewrites the strategy in the background,
    # so the next game can start straight away.
    import subprocess
    import sys
    with (MEMORY / 'master.log').open('ab') as fh:
        subprocess.Popen([sys.executable, str(HERE / 'master.py'), str(arch), '--model', os.environ.get('SG_MASTER_MODEL', ARGS.model)],
                         stdout=fh, stderr=fh, start_new_session=True)
    print('[commander] master agent started on this match (log: llm-bridge/memory/master.log)', flush=True)


def game_alive():
    if not ARGS.game_pid:
        return True
    try:
        os.kill(ARGS.game_pid, 0)
        return True
    except OSError:
        return False


def ensure_ollama():
    """Start Ollama if it is not running (for games started from the menu)."""
    try:
        urllib.request.urlopen(OLLAMA + '/api/tags', timeout=2).read()
        return
    except Exception:
        pass
    exe = shutil.which('ollama') or next((p for p in ('/opt/homebrew/bin/ollama', '/usr/local/bin/ollama') if os.path.exists(p)), None)
    if not exe:
        print('[commander] Ollama is not installed; the AI plays as Brutal Plus without Qwen', flush=True)
        return
    import subprocess
    subprocess.Popen([exe, 'serve'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    for _ in range(40):
        time.sleep(0.5)
        try:
            urllib.request.urlopen(OLLAMA + '/api/tags', timeout=2).read()
            print('[commander] started Ollama', flush=True)
            return
        except Exception:
            pass


def main():
    run = Path(ARGS.dir).resolve()
    run.mkdir(parents=True, exist_ok=True)
    pidfile = run / 'commander.pid'
    try:  # only one commander per folder (the game and a launcher may both try to start one)
        other = int(pidfile.read_text().strip())
        if other != os.getpid():
            os.kill(other, 0)
            print('[commander] another commander (pid %d) is already running; exiting' % other, flush=True)
            return
    except (OSError, ValueError):
        pass
    pidfile.write_text(str(os.getpid()))
    ensure_ollama()
    if not (MEMORY / 'strategy.md').exists() and (MEMORY / 'strategies/v001.md').exists():
        shutil.copyfile(MEMORY / 'strategies/v001.md', MEMORY / 'strategy.md')  # first run: the starting strategy
    ledger = run / 'ledger.json'
    log = run / 'match.jsonl'
    print('[commander] model %s, watching %s' % (ARGS.model, ledger), flush=True)
    try:  # warm the model up so the first real answer is fast
        ollama([{'role': 'user', 'content': 'Reply {"ok":1}'}], timeout=60, num_predict=8)
    except Exception as exc:
        print('[commander] cannot reach Ollama at %s: %s' % (OLLAMA, exc), flush=True)
    seen = 0.0
    seq = 0
    decisions = []
    last_frame = -1
    last_ask = -10 ** 9
    offset = 0
    pending = []  # events from every ledger since we last asked (none get lost while the model is busy)
    while True:
        time.sleep(0.2)
        try:
            mtime = ledger.stat().st_mtime
        except FileNotFoundError:
            if not game_alive():
                return
            continue
        if mtime == seen:
            if decisions and (time.time() - mtime > ARGS.match_gap or not game_alive()):
                end_of_match(run, decisions)
                decisions, pending = [], []
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
            decisions, pending, last_ask, offset = [], [], -10 ** 9, 0
        last_frame = L['frame']
        try:
            size = log.stat().st_size
            if size < offset:
                offset = 0
            with log.open('rb') as fh:
                fh.seek(offset)
                chunk = fh.read()
            cut = chunk.rfind(b'\n') + 1
            offset += cut
            for line in chunk[:cut].decode('utf-8', 'replace').splitlines():
                try:
                    pending += json.loads(line).get('events', [])
                except ValueError:
                    pass
        except FileNotFoundError:
            pass
        urgent = bool(pending)
        if decisions and not urgent and L['frame'] - last_ask < ARGS.every_frames:
            continue  # nothing happened: ask every ~ARGS.every_frames frames
        held = (None, 0)  # current stance and how many minutes it has been held
        if decisions:
            i = len(decisions) - 1
            while i > 0 and decisions[i - 1]['stance'] == decisions[-1]['stance']:
                i -= 1
            held = (decisions[-1]['stance'], L['min'] - decisions[i]['min'])
        events, pending = pending, []
        report = describe(L, events, held)
        messages = [{'role': 'system', 'content': SYSTEM + memory_text()}, {'role': 'user', 'content': report}]
        last_ask = L['frame']
        t0 = time.time()
        try:
            raw = ollama(messages, timeout=ARGS.timeout)
            order = clean(raw, L)
            # Safety rule: after minute 10 with money banked, "expand" means attack.
            if order['stance'] == 'expand' and L['min'] >= 10 and L['credits'] > 5000:
                order['stance'] = 'attack'
                order['note'] = '(expand overruled: money banked) ' + order['note']
        except Exception as exc:
            pending = events + pending  # try again with the same news
            print('[commander] min %d: no order (%s); the game keeps playing on its own' % (L['min'], exc), flush=True)
            continue
        took = time.time() - t0
        seq += 1
        order['seq'] = seq
        write_atomic(run / 'orders.json', json.dumps(order))
        decisions.append(dict(order, min=L['min'], frame=L['frame'], seconds=round(took, 2), events=events))
        with (run / 'decisions.jsonl').open('a') as fh:
            fh.write(json.dumps(decisions[-1]) + '\n')
        print('[commander] min %d (%.1fs)%s: %s / %s / %s%s%s%s - %s' % (
            L['min'], took, ' after [' + '; '.join(events[-3:]) + ']' if events else '',
            order['stance'], order['target'], order['build'],
            ', raid %d' % order['raid'] if order['raid'] else '', ', scout' if order['scout'] else '',
            ', navy ' + order['navy'] if order['navy'] != 'off' else '', order['note']), flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--model', default=os.environ.get('SG_LLM_MODEL', 'qwen3:4b'))
    p.add_argument('--dir', default=str(HERE / 'run'))
    p.add_argument('--timeout', type=float, default=6.0, help='seconds to wait for each answer (the game never waits)')
    p.add_argument('--every-frames', type=int, default=150, help='when nothing happens, ask every N game frames (150 = 10 s)')
    p.add_argument('--game-pid', type=int, default=0, help='stop when this process ends')
    p.add_argument('--match-gap', type=float, default=20.0, help='seconds without ledgers = match over')
    ARGS = p.parse_args()
    main()
