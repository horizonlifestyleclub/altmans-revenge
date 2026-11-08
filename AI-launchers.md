# Choose your opponent

**Showcase All New Units.command** opens a populated sandbox automatically: a complete base with Colossus, all five Chinese data centres, Area 51 and its four aircraft, EMP buildings, drone operators at all three ranks, two deployed drone tanks, and a nearby enemy target force. The map is revealed for inspection. The operators launch their own drones with the normal replacement rules. Singularity research is ready to start through **F8**; it has not already completed. This is a unit showcase, not a fair AI match. It uses an isolated executable, map and settings under `unit-showcase`; it does not change the original opponents. Always use this launcher to start a fresh showcase, since custom drone/research state does not yet support reliable save/load.

**Play Original AI.command** opens the Singularity mod with the original **Easy Enemy, Medium Enemy and Brutal Enemy** choices. The new units, portraits and factions remain available. Original AI scripts and difficulty behaviour are preserved.

**Brutal Plus AI.command** opens the Singularity mod against **Brutal Plus**. It is a harder scripted opponent whose design ideas come from Mental Omega's AI; none of Mental Omega's files are used. In the skirmish menu, pick **Brutal Plus** (strongest), **Brutal Plus Veteran** or **Brutal Plus Recruit** as the opponent's difficulty.
- **Army:** it builds mixed armies, not single-unit spam. Kirovs come at most as one pair at a time.
- **Scouting and raids:** it sends scouts and small raids against your ore trucks.
- **Counter-building:** it answers what you build. Kirovs or bombers bring anti-air; Prism tanks or a Bastion bring heavy armour; defence towers bring artillery.
- **Attacks:** it gathers its army at a rally point and attacks in one big wave once the army is large enough. A beaten wave pulls back as a group.
- **Defence:** home guards stay in the base. Anything entering the base is engaged, and an army still marching out is recalled if the base is attacked hard.
- **New units:**
  - America/Britain: LINK operators, Bastion, Colossus, AEGIS arrays and B-2s.
  - China/Confederation: drone-nest operators, the drone factory with Locusts, the compute nodes and interference arrays.
- **Extras:** it adds second factories when rich.
- **Cheats:** like the classic Brutal AI, it sees the whole map and gets the game's own AI money bonus. Only the top **Brutal Plus** slot also gets +$300 a minute (+$600 once you start a Singularity, plus about $75 a minute per refinery). **Brutal Plus Veteran** gets no extra money. Its standing army is capped at about 12 units plus 2 per minute, up to 60.
- **Classic Brutal (original AI)** in the easiest slot is the original AI, for comparison matches. **Watch Brutal Plus vs Classic.command** runs one with you as a spectator.

For a quick test against a fresh opponent (you Allies, Brutal Plus China, $10,000, MCV only), run `.venv/bin/python mod-src/launch_brutalplus_test.py`. Add `--enemy 9` for Yuri or `--enemy 0` for America. The AI writes its decisions to `brutalplus-playtest/test-game.log`, in lines starting with `[brutalplus]`.

**Test Adaptive AI.command** starts a separate, fresh skirmish automatically on **The Alamo (2)**: you play **Allies**, against **Soviets / Adaptive Commander**. Both start with **$10,000 and an MCV**, with crates disabled. Deploy your MCV to start building. The game uses normal production and costs; no test-only invulnerability or free buildings are added. This is the existing experimental local opponent, with no API bill. It remains limited to Allied/Soviet land play and does not yet use its own Singularity research, native aircraft production or automatic superweapons.

The test has private settings and its own executable location under `adaptive-playtest`. It shares the installed artwork and rules so future art updates carry through. Its launcher refreshes the executable copy when the main build changes and resets only its private skirmish setup. The normal Singularity and Adaptive Commander menu launchers still work as before.

Custom drone and AI state still does not support reliable save/load; use a fresh test match. Quit the test normally when finished. Starting the test does not close any existing match; finish or exit your other match first if you only want one game window.

Prepared locally. No commit, push, pull request or deployment. The test is available on demand and is not left running automatically.

## Brutal Plus + Qwen commander (branch v10/llm)

- **Brutal Plus Qwen.command**: you vs Brutal Plus, with its strategy steered by a local Qwen model.
- **Watch Qwen vs Classic.command**: the Qwen-steered Brutal Plus vs the original Brutal AI; you watch.
- Needs Ollama and the model (`brew install ollama`, `ollama pull qwen3:4b`). The launchers start Ollama if it isn't running.
- Details, files and the learning playbook: `llm-bridge/README.md`.
