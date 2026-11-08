#!/bin/bash
# Removes only the folders Altman's Revenge installed (they carry a
# .altmans-revenge marker) and puts back the newest backup of each, if any.
#   "Uninstall Altman's Revenge.command" [<recomp folder>] [--yes]
set -e
TARGET=""; YES=0
for a in "$@"; do case "$a" in --yes) YES=1;; *) TARGET="$a";; esac; done
echo; echo "   ALTMAN'S REVENGE - uninstall"; echo
if [ -z "$TARGET" ]; then read -r -p "Drag the Red Alert 2 recomp folder here and press Enter: " TARGET; TARGET="${TARGET%\'}"; TARGET="${TARGET#\'}"; TARGET="${TARGET% }"; fi
ROOT="$(cd "$TARGET" && pwd)"
n=0
for dst in "$ROOT/mods/yr/Singularity" "$ROOT/mods/yr/Singularity - Brutal Plus" "$ROOT/llm-bridge"; do
  if [ -f "$dst/.altmans-revenge" ]; then
    rm -rf "$dst"; n=$((n+1)); echo "  removed ${dst#$ROOT/}"
    b="$(ls -d "$dst".backup-* 2>/dev/null | sort | tail -1)"
    [ -n "$b" ] && mv "$b" "$dst" && echo "  restored ${dst#$ROOT/} from $(basename "$b")"
  fi
done
[ $n = 0 ] && echo "  Nothing from Altman's Revenge found in $ROOT."
echo "Ollama and the Qwen model are left installed (remove with: ollama rm qwen3:4b)."
[ $YES = 1 ] || read -r -p "Press Enter to close. " _
