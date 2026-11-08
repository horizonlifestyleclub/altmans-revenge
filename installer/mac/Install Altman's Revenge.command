#!/bin/bash
# Altman's Revenge - installer for macOS (works on Linux too).
#
# What it changes, and nothing else:
#   <recomp>/mods/yr/Singularity/                 the mod
#   <recomp>/mods/yr/Singularity - Brutal Plus/   the AI profile (Qwen Commander slot)
#   <recomp>/llm-bridge/                          the local Qwen commander
# <recomp> is the folder holding the game's build folder (build-macos/, build/).
# Your Red Alert 2 game files are never touched. Folders of the same name that
# were not put there by this installer are moved aside to *.backup-<date>, so
# Uninstall can put them back.
#
#   "Install Altman's Revenge.command" [<recomp folder or its game executable>] [--yes] [--no-qwen]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$HERE"; [ -d "$PKG/payload" ] || PKG="$(cd "$HERE/.." && pwd)"
PAYLOAD="$PKG/payload"
VERSION="$(cat "$PAYLOAD/VERSION" 2>/dev/null || echo dev)"
MODEL="qwen3:4b"
YES=0; QWEN=1; TARGET=""
for a in "$@"; do case "$a" in --yes) YES=1;; --no-qwen) QWEN=0;; *) TARGET="$a";; esac; done
ask() { [ $YES = 1 ] && return 0; read -r -p "$1 [y/N] " r; [[ "$r" =~ ^[Yy] ]]; }

cat <<'BANNER'

   =====================================================
      A L T M A N ' S   R E V E N G E
      a Singularity mod for Red Alert 2: Yuri's Revenge
   =====================================================

BANNER
echo "   version $VERSION"
echo
[ -d "$PAYLOAD/mods/yr/Singularity" ] || { echo "The mod files are missing next to this installer ($PAYLOAD)."; exit 1; }

# ---- find the recompiled game ----
if [ -z "$TARGET" ]; then
  for c in "$HOME/Applications/RA2-Recomp" "$HOME/RA2-Recomp" "$HOME/redalert2-recomp" "/Applications/RA2-Recomp"; do
    [ -d "$c" ] && { TARGET="$c"; break; }; done
  echo "Drag the Red Alert 2 recomp folder (the one with build-macos/ in it) into this window,"
  read -r -p "then press Enter${TARGET:+ (Enter alone: $TARGET)}: " in
  in="${in%\'}"; in="${in#\'}"; in="${in% }"; [ -n "$in" ] && TARGET="$in"
fi
[ -f "$TARGET" ] && TARGET="$(cd "$(dirname "$TARGET")/.." && pwd)"
TARGET="$(cd "$TARGET" 2>/dev/null && pwd)" || { echo "Not a folder: $TARGET"; exit 1; }
EXE=""
for d in build-macos build-linux build .; do for n in ra2-singularity ra2; do
  [ -z "$EXE" ] && [ -x "$TARGET/$d/$n" ] && [ ! -d "$TARGET/$d/$n" ] && EXE="$TARGET/$d/$n"; done; done
if [ -z "$EXE" ]; then
  echo "No recompiled game (build-macos/ra2) in $TARGET."
  echo "Install and build Red Alert 2 recomp first, then run this again."
  exit 1
fi
ROOT="$(cd "$(dirname "$EXE")/.." && pwd)"
echo "Game build: $EXE"
if ! grep -aq "Run the Altman's Revenge installer" "$EXE"; then
  echo "Note: this game build is older than Altman's Revenge, so drones, kamikaze units"
  echo "      and the Qwen Commander stay off. Update and rebuild the recomp (run its"
  echo "      setup again) and they switch on; the rest of the mod works now."
fi
GAME="$ROOT/game"; [ -d "$GAME" ] || GAME=""
if [ -z "$GAME" ] || ! ls "$GAME" | grep -iqxE 'langmd\.mix|audio\.idx'; then
  read -r -p "Red Alert 2 game folder (with langmd.mix; Enter to skip the new sounds): " GAME
  GAME="${GAME%\'}"; GAME="${GAME#\'}"; GAME="${GAME% }"
fi

# ---- install, moving foreign folders aside ----
STAMP="$(date +%Y%m%d-%H%M%S)"
place() { # $1 payload path, $2 destination
  local src="$1" dst="$2"
  if [ -e "$dst" ]; then
    if [ -f "$dst/.altmans-revenge" ]; then rm -rf "$dst"
    else mv "$dst" "$dst.backup-$STAMP"; echo "  kept your existing $(basename "$dst") as $(basename "$dst").backup-$STAMP"; fi
  fi
  mkdir -p "$(dirname "$dst")"; cp -R "$src" "$dst"; echo "$VERSION" > "$dst/.altmans-revenge"
}
echo; echo "Installing into $ROOT"
for m in "$PAYLOAD"/mods/yr/*/; do m="${m%/}"; place "$m" "$ROOT/mods/yr/$(basename "$m")"; echo "  mods/yr/$(basename "$m")"
  # the mod's new sounds, merged into your own copy of the game's sound bank
  if [ -n "$GAME" ] && command -v python3 >/dev/null; then
    python3 "$PAYLOAD/merge_audio.py" "$GAME" "$PAYLOAD/sounds/patch.idx" "$ROOT/mods/yr/$(basename "$m")" | sed 's/^/    /' ||
      echo "    (new sounds skipped: the game plays its own)"
  fi
done
[ -n "$GAME" ] && command -v python3 >/dev/null || echo "  (new sounds skipped: needs python3 and your game folder; the game plays its own sounds)"
# The commander keeps what it learned (memory/) across reinstalls.
if [ -d "$ROOT/llm-bridge/memory" ] && [ -f "$ROOT/llm-bridge/.altmans-revenge" ]; then
  mv "$ROOT/llm-bridge/memory" "$ROOT/.ar-memory-$STAMP"; place "$PAYLOAD/llm-bridge" "$ROOT/llm-bridge"
  rm -rf "$ROOT/llm-bridge/memory"; mv "$ROOT/.ar-memory-$STAMP" "$ROOT/llm-bridge/memory"
else place "$PAYLOAD/llm-bridge" "$ROOT/llm-bridge"; fi
echo "  llm-bridge (Qwen commander)"

# ---- Qwen: required for the Qwen Commander opponent ----
if [ $QWEN = 1 ]; then
  echo; echo "Qwen Commander (the learning AI) needs Python 3, Ollama and the $MODEL model (about 2.5 GB)."
  command -v python3 >/dev/null || echo "  ! python3 is missing: install Xcode's command line tools (xcode-select --install)."
  OLL="$(command -v ollama || true)"; for p in /opt/homebrew/bin/ollama /usr/local/bin/ollama /Applications/Ollama.app/Contents/Resources/ollama; do [ -z "$OLL" ] && [ -x "$p" ] && OLL="$p"; done
  if [ -z "$OLL" ]; then
    if command -v brew >/dev/null && ask "  Ollama is not installed. Install it with Homebrew now?"; then brew install ollama; OLL="$(command -v ollama)"
    else echo "  Install Ollama from https://ollama.com/download, then run this installer again."; fi
  fi
  if [ -n "$OLL" ]; then
    curl -s -m 2 http://127.0.0.1:11434/api/tags >/dev/null 2>&1 || { ("$OLL" serve >/dev/null 2>&1 &); sleep 3; }
    if "$OLL" list 2>/dev/null | awk '{print $1}' | grep -qx "$MODEL"; then echo "  $MODEL is ready."
    elif ask "  Download the $MODEL model now (about 2.5 GB)?"; then "$OLL" pull "$MODEL"
    else echo "  Later: ollama pull $MODEL   (until then the Qwen slot plays as Brutal Plus and says so)"; fi
  fi
fi

echo
echo "Done. Start the game, press F9 at the main menu until the title says"
echo "'Singularity - Brutal Plus', and pick 'Qwen Commander' as an AI's difficulty."
echo "To remove it: 'Uninstall Altman's Revenge.command'."
[ $YES = 1 ] || read -r -p "Press Enter to close. " _
