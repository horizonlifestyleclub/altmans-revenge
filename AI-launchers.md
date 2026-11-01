# Choose your opponent

**Showcase All New Units.command** opens a populated sandbox automatically: a complete base with Colossus, all five Chinese data centres, Area 51 and its four aircraft, EMP buildings, drone operators at all three ranks, two deployed drone tanks, and a nearby enemy target force. The map is revealed for inspection. The operators launch their own drones with the normal replacement rules. Singularity research is ready to start through **F8**; it has not already completed. This is a unit showcase, not a fair AI match. It uses an isolated executable, map and settings under `unit-showcase`; it does not change the original opponents. Always use this launcher to start a fresh showcase, since custom drone/research state does not yet support reliable save/load.

**Play Original AI.command** opens the Singularity mod with the original **Easy Enemy, Medium Enemy and Brutal Enemy** choices. The new units, portraits and factions remain available. Original AI scripts and difficulty behaviour are preserved.

**Test Adaptive AI.command** starts a separate, fresh skirmish automatically on **The Alamo (2)**: you play **Allies**, against **Soviets / Adaptive Commander**. Both start with **$10,000 and an MCV**, with crates disabled. Deploy your MCV to start building. The game uses normal production and costs; no test-only invulnerability or free buildings are added. This is the existing experimental local opponent, with no API bill. It remains limited to Allied/Soviet land play and does not yet use its own Singularity research, native aircraft production or automatic superweapons.

The test has private settings and its own executable location under `adaptive-playtest`. It shares the installed artwork and rules so future art updates carry through. Its launcher refreshes the executable copy when the main build changes and resets only its private skirmish setup. The normal Singularity and Adaptive Commander menu launchers still work as before.

Custom drone and AI state still does not support reliable save/load; use a fresh test match. Quit the test normally when finished. Starting the test does not close any existing match; finish or exit your other match first if you only want one game window.

Prepared locally. No commit, push, pull request or deployment. The test is available on demand and is not left running automatically.
