# Altman's Revenge

A Singularity mod for the Red Alert 2: Yuri's Revenge recompilation: drone
warfare, AI data centres, the Singularity race, a learning Qwen opponent, and a
navy that fights back with Kamikaze Dolphins and Sea Drones.

## What you need

- **Red Alert 2 recomp**, installed and built on your machine from your own copy
  of the game (the recomp's installer does this).
- **For the Qwen Commander opponent** (required for that opponent only):
  Python 3, [Ollama](https://ollama.com/download) and the `qwen3:4b` model,
  about 2.5 GB. The installer checks for each one and offers to install it.
  Without them the Qwen slot plays as Brutal Plus and says so on screen.

## Install

- **macOS**: double-click `Install Altman's Revenge.command` (first time:
  right-click > Open). Drag your recomp folder into the window when asked.
- **Windows**: double-click `Install-AltmansRevenge.cmd` and paste the path of
  your recomp folder (the one with `build\ra2.exe`).

## What it changes

Only three folders next to the game's build folder:
`mods/yr/Singularity`, `mods/yr/Singularity - Brutal Plus` and `llm-bridge`.
Your game files and the plain game are not touched: choose the mod in the game
(macOS: F9 at the main menu; Windows: F10 > Mod), or `--mod none` to play the
original. If a folder with one of those names already exists, it is kept as
`<name>.backup-<date>` and the uninstaller puts it back.

## Uninstall

`Uninstall Altman's Revenge.command` (macOS) or `Uninstall-AltmansRevenge.cmd`
(Windows). Ollama and the model stay installed: `ollama rm qwen3:4b` removes it.

## New in this version (v11)

- **Kamikaze Dolphin** (Allies, Naval Yard, $400): a dolphin with a limpet
  charge. One hit sinks a destroyer; it is gone after the hit.
- **Sea Drone** (China and the other Soviet sides, Naval Yard, $350): an
  unmanned explosive boat that rams ships and shore buildings.
- The **Qwen Commander's navy** builds both when it goes to sea.

## Platform status

| | macOS | Windows |
|---|---|---|
| Installer | yes | yes |
| New rules, units, AI profiles | yes | yes |
| New sounds (merged into your own copy of the game's sound bank) | yes | yes |
| Native Singularity runtime (drone control, kamikaze clean-up, Singularity research) | yes (any build of this version) | not yet |
| Qwen Commander | yes | not yet (needs the runtime) |
