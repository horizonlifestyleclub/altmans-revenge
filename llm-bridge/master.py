"""Master agent: reviews a finished match and rewrites the strategy.

  python3 llm-bridge/master.py <match archive folder> [--model qwen3:4b]

Started automatically by commander.py after every match (in the background,
so you can start the next game straight away; the commander picks up the new
strategy as soon as it is written). It:

1. works out the facts in Python: the result (from score.jsonl, the
   score-screen numbers), how the opponent played (what it built and when,
   when and how hard it attacked, as far as our side saw it), our waves,
   raids, scouting and losses;
2. asks the model, thinking step by step, for a review of the match
   (saved as review.md in the match folder);
3. asks it for an improved strategy and an updated opponent profile, as JSON,
   checked before saving. Files in llm-bridge/memory/:
     strategy.md        the plan the commander follows next game
     opponent.md        how the opponent (you) plays, built up over matches
     strategy-log.md    one line per match: strategy version, result, length
     strategies/vNNN.md every strategy version, so a good one is never lost
   If the last two versions both lost and an older one won, the master is
   told so and asked to go back toward the winner.
Only the Python standard library is used.
"""
import argparse
import json
import os
import re
import time
import urllib.request
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
MEMORY = Path(os.environ.get('SG_MEMORY_DIR', HERE / 'memory'))
OLLAMA = os.environ.get('OLLAMA_HOST', 'http://127.0.0.1:11434')


def ollama(model, messages, timeout, num_predict, json_mode, think):
    body = {'model': model, 'messages': messages, 'stream': False, 'think': think, 'keep_alive': '30m',
            'options': {'temperature': 0.4, 'num_predict': num_predict, 'num_ctx': 16384}}
    if json_mode:
        body['format'] = 'json'
    req = urllib.request.Request(OLLAMA + '/api/chat', json.dumps(body).encode(), {'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)['message']['content']


def read_jsonl(path):
    rows = []
    if path.exists():
        for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
            try:
                rows.append(json.loads(line))
            except ValueError:
                pass
    return rows


def result_of(scores):
    if not scores:
        return 'unknown', 'no score recorded'
    last = scores[-1]
    me = next((p for p in last['players'] if p['who'] == 'qwen'), None)
    foes = [p for p in last['players'] if p['enemy']]
    if not me:
        return 'unknown', 'no score for our side'
    table = '; '.join('%s: %d buildings, %d units ($%d)' % (p['who'], p['buildings'], p['units'], p['value']) for p in last['players'])
    if me['buildings'] == 0:
        return 'LOSS', table
    if foes and all(p['buildings'] == 0 for p in foes):
        return 'WIN', table
    mine = me['value'] + 300 * me['buildings']
    theirs = sum(p['value'] + 300 * p['buildings'] for p in foes)
    return ('UNFINISHED, we were ahead' if mine > theirs else 'UNFINISHED, we were behind'), table


def facts(rows, decisions, scores):
    """Everything the model needs, worked out exactly in Python."""
    out = []
    first = {}
    for r in rows:
        for e in r.get('events', []):
            m = re.match(r'first sighting of enemy (unit|building) (\S+)', e)
            if m and m.group(2) not in first:
                first[m.group(2)] = r['min']
    if first:
        out.append('Opponent units/buildings, minute first seen: ' + ', '.join('%s@%d' % kv for kv in sorted(first.items(), key=lambda kv: kv[1])))
    attacked = [r['min'] for r in rows if any('base is being attacked' in e for e in r.get('events', []))]
    if attacked:
        out.append('Our base was attacked at minutes: ' + ', '.join(str(m) for m in sorted(set(attacked))))
    else:
        out.append('Our base was never attacked.')
    peak = max(rows, key=lambda r: r['enemy_army_known']['value'])
    if peak['enemy_army_known']['value']:
        k = peak['enemy_army_known']
        out.append('Biggest enemy army we saw: %d units $%d at minute %d (%s).' % (
            k['units'], k['value'], peak['min'], ', '.join('%s %d' % kv for kv in sorted(k['types'].items(), key=lambda kv: -kv[1])[:8])))
    types = Counter()
    for r in rows:
        for t, n in r['enemy_army_known']['types'].items():
            types[t] = max(types[t], n)
    if types:
        out.append('Enemy unit mix (most seen at once): ' + ', '.join('%s %d' % kv for kv in types.most_common(10)))
    waves = [e for r in rows for e in r.get('events', []) if 'wave' in e]
    if waves:
        out.append('Our waves: ' + '; '.join('min %d %s' % (r['min'], e) for r in rows for e in r.get('events', []) if 'wave' in e)[:900])
    raids = [(r['min'], e) for r in rows for e in r.get('events', []) if 'raid' in e]
    if raids:
        out.append('Raids: ' + '; '.join('min %d %s' % x for x in raids)[:600])
    scouts = [(r['min'], e) for r in rows for e in r.get('events', []) if 'scout' in e]
    if scouts:
        out.append('Scouting: ' + '; '.join('min %d %s' % x for x in scouts)[:600])
    lostb = [(r['min'], e) for r in rows for e in r.get('events', []) if 'lost a building' in e or 'lost a miner' in e]
    if lostb:
        out.append('Our losses of note: ' + '; '.join('min %d %s' % x for x in lostb)[:500])
    out.append('Our army value by minute: ' + ', '.join('%d:$%d' % (r['min'], r['army']['value']) for r in rows if r['frame'] % 2700 < 45))
    out.append('Our credits by minute: ' + ', '.join('%d:$%d' % (r['min'], r['credits']) for r in rows if r['frame'] % 2700 < 45))
    for s in scores[::3]:
        out.append('Score minute %d: ' % s['min'] + '; '.join('%s %d bldg %d units $%d' % (p['who'], p['buildings'], p['units'], p['value']) for p in s['players']))
    changes, prev = [], None
    for d in decisions:
        key = (d['stance'], d['target'], d['build'], d.get('raid', 0))
        if key != prev:
            changes.append('min %d %s/%s/%s raid %d (%s)' % (d['min'], *key, d['note']))
            prev = key
    out.append('Commander orders when they changed: ' + '; '.join(changes)[:1500])
    return '\n'.join('- ' + x for x in out)


def versions():
    folder = MEMORY / 'strategies'
    folder.mkdir(parents=True, exist_ok=True)
    return sorted(folder.glob('v*.md'))


def log_lines():
    p = MEMORY / 'strategy-log.md'
    return [l for l in p.read_text().splitlines() if l.startswith('| v')] if p.exists() else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('match')
    ap.add_argument('--model', default=os.environ.get('SG_MASTER_MODEL', os.environ.get('SG_LLM_MODEL', 'qwen3:4b')))
    args = ap.parse_args()
    arch = Path(args.match)
    rows = read_jsonl(arch / 'match.jsonl')
    scores = read_jsonl(arch / 'score.jsonl')
    decisions = json.loads((arch / 'decisions.json').read_text()) if (arch / 'decisions.json').exists() else []
    if not rows or rows[-1]['min'] < 5:
        print('[master] match too short to learn from', flush=True)
        return
    result, table = result_of(scores)
    strategy = (MEMORY / 'strategy.md').read_text() if (MEMORY / 'strategy.md').exists() else '(no strategy yet: this is the first match)'
    opponent = (MEMORY / 'opponent.md').read_text() if (MEMORY / 'opponent.md').exists() else '(nothing known yet)'
    core = (MEMORY / 'core-rules.md').read_text() if (MEMORY / 'core-rules.md').exists() else ''
    vs = versions()
    current = vs[-1].stem if vs else 'v000'
    history = log_lines()
    advice = ''
    results = [l.split('|')[3].strip() for l in history]
    if len(results) >= 2 and all(r.startswith('LOSS') or 'behind' in r for r in results[-2:]):
        wins = [l.split('|')[1].strip() for l in history if l.split('|')[3].strip().startswith('WIN') or 'ahead' in l.split('|')[3]]
        if wins:
            advice = ('The last two strategies lost. Strategy %s did well; here it is, move back toward it:\n%s\n' % (
                wins[-1], (MEMORY / 'strategies' / (wins[-1] + '.md')).read_text() if (MEMORY / 'strategies' / (wins[-1] + '.md')).exists() else ''))
    report = 'RESULT: %s (%d game minutes). Final score: %s\n\nWhat happened:\n%s' % (result, rows[-1]['min'], table, facts(rows, decisions, scores))
    (arch / 'facts.md').write_text(report + '\n')
    print('[master] reviewing match: %s, %d minutes' % (result, rows[-1]['min']), flush=True)

    t0 = time.time()
    review = ollama(args.model, [
        {'role': 'system', 'content': 'You are the head coach of an RTS (Red Alert 2) AI commander. Think it through, then '
         'write a short, honest review of the match: 1) how the opponent played (build, timing, unit mix, attacks), '
         '2) what our strategy got right, 3) what lost us time, units or the game, 4) the 3 most important changes. '
         'Use only the facts given. Under 300 words.'},
        {'role': 'user', 'content': 'Strategy used this match (%s):\n%s\n\nOpponent profile before this match:\n%s\n\n%s' % (current, strategy, opponent, report)}],
        timeout=600, num_predict=3000, json_mode=False, think=True).strip()
    (arch / 'review.md').write_text('# Review (%s, strategy %s)\n\n%s\n' % (result, current, review))
    print('[master] review written in %.0fs' % (time.time() - t0), flush=True)

    raw = ollama(args.model, [
        {'role': 'system', 'content': 'You write the strategy an RTS AI commander follows in its NEXT match against the same '
         'opponent, and a profile of that opponent. The commander can only choose: stance attack/defend/harass/expand/retreat, '
         'target core/factory/refinery/conyard/army/defences or a building ID, build armour/infantry/antiair/artillery or a unit ID, '
         'scout now, raid 0-8 units on miners. Its units fight on their own. Keep what worked, fix what failed, be concrete '
         '(minutes, army values, counts). Do not repeat these fixed core rules:\n' + core + '\n'
         'Reply ONLY with JSON: {"name": "short strategy name", '
         '"opening": ["2-4 rules for minutes 0-6"], "midgame": ["2-5 rules"], "counters": ["2-5 if-you-see-X-do-Y rules"], '
         '"triggers": ["2-4 rules for when to attack, defend or retreat"], '
         '"opponent": ["3-8 facts about how this opponent plays, kept from the old profile if still true"]}. '
         'Each rule one sentence under 30 words.'},
        {'role': 'user', 'content': '%sCurrent strategy (%s):\n%s\n\nCurrent opponent profile:\n%s\n\n%s\n\nCoach review:\n%s' % (
            advice, current, strategy, opponent, report, review)}],
        timeout=300, num_predict=1500, json_mode=True, think=False)
    d = json.loads(raw)

    def rules(key, lo, hi):
        items = [str(x).strip() for x in d.get(key, []) if 8 < len(str(x).strip()) < 260]
        if not lo <= len(items) <= hi + 2:
            raise ValueError('%s: %d rules' % (key, len(items)))
        return items[:hi]
    parts = {k: rules(k, 1, n) for k, n in [('opening', 4), ('midgame', 5), ('counters', 5), ('triggers', 4)]}
    opp = rules('opponent', 1, 8)
    num = int(current[1:]) + 1 if vs else 1
    name = 'v%03d' % num
    text = '# Strategy %s: %s\n\n(written by the master agent after a %s; follow it unless the situation clearly says otherwise)\n\n' % (
        name, str(d.get('name', 'unnamed'))[:60], result)
    for title, key in [('Opening (minutes 0-6)', 'opening'), ('Middle game', 'midgame'), ('Counters', 'counters'), ('When to attack, defend, retreat', 'triggers')]:
        text += '## %s\n' % title + ''.join('- %s\n' % r for r in parts[key]) + '\n'
    (MEMORY / 'strategies').mkdir(parents=True, exist_ok=True)
    (MEMORY / 'strategies' / (name + '.md')).write_text(text)
    (MEMORY / 'strategy.md').write_text(text)
    (MEMORY / 'opponent.md').write_text('# How the opponent plays (updated after every match)\n\n' + ''.join('- %s\n' % r for r in opp))
    logp = MEMORY / 'strategy-log.md'
    if not logp.exists():
        logp.write_text('# Strategy log\n\n| used | match | result | minutes | next |\n|---|---|---|---|---|\n')
    with logp.open('a') as fh:
        fh.write('| %s | %s | %s | %d | %s |\n' % (current, arch.name, result, rows[-1]['min'], name))
    (arch / 'strategy-after.md').write_text(text)
    print('[master] new strategy %s "%s" written (%.0fs total)' % (name, d.get('name', ''), time.time() - t0), flush=True)


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print('[master] failed, strategy unchanged: %s' % exc, flush=True)
