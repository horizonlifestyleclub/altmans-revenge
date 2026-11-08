# Qwen commander for Singularity - Brutal Plus

A local language model (default **Qwen3-4B** through Ollama, running on this Mac,
no internet needed once installed) steers the top Brutal Plus AI's strategy.
The Brutal Plus rules still do the moment-to-moment fighting; the model only
picks a **stance**, a **target** and a **build focus** every ~10 seconds.

How the pieces talk (plain files, so the game never waits):

| File (in `llm-bridge/run/`) | Written by | What it is |
|---|---|---|
| `ledger.json` | game, every 3 s | what that AI can **see**: its army, base, money, and enemy units/buildings in its sight |
| `match.jsonl` | game | every ledger of the current match (the match log) |
| `orders.json` | commander | `{"stance","target","build","note"}` |
| `decisions.jsonl`, `commander.log` | commander | every answer and how long it took |

If the model is slow (over 3 s) that answer is skipped; if no orders arrive for
60 s the AI plays on its own rules again.

What the commander knows (only what its side has seen): enemies in sight now; a
memory of enemy bases (kept until seen gone) and armies (2 minutes); scout
reports from each enemy start area; its own losses, kills and wave progress.
It is asked every 10 s, and at once when something happens (base hit, wave
fighting, new enemy type, scout finds a base, building lost). Orders: stance,
target (or a named enemy building), build (or a named unit), scout, raid 0-8.

Learning loop (in `memory/`), automatic after every match of 5+ minutes:
1. the commander archives the match in `matches/<date>/` (ledgers, orders, score);
2. `master.py` (the master agent) starts in the background: it works out the
   facts (result, what the opponent built and when, its attacks, our waves),
   writes a coach's review (`review.md` in the match folder), then a new
   `strategy.md` and an updated `opponent.md` (how you play);
3. every version is kept in `strategies/vNNN.md`, with results in
   `strategy-log.md`; after two losses it is pointed back at the last winner.
The commander re-reads `core-rules.md` + `strategy.md` + `opponent.md` on every
question, so the next game uses the new plan as soon as it is written (about a
minute after the match). `strategies/v001.md` is the hand-written start.
Learned files stay on this Mac (not in git).

Run: double-click **Brutal Plus Qwen.command** (you vs it) or
**Watch Qwen vs Classic.command** (it vs the original Brutal AI).
Another model: `ollama pull qwen3:8b`, then add `--model qwen3:8b` (live commander) or set
`SG_MASTER_MODEL=qwen3:8b` (the after-match master only, where speed does not matter).
