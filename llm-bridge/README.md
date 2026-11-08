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

Learning loop (in `memory/`): after each match the commander writes
`last-match.md`, archives the match in `matches/<date>/`, and asks the model
to rewrite `playbook.md` (about one page), which is fed to it every turn.

Run: double-click **Brutal Plus Qwen.command** (you vs it) or
**Watch Qwen vs Classic.command** (it vs the original Brutal AI).
Another model: `ollama pull qwen3:8b`, then add `--model qwen3:8b`.
