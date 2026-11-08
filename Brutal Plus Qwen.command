#!/bin/sh
# You vs Brutal Plus steered by the local Qwen commander.
# Needs Ollama (brew install ollama) and the model (ollama pull qwen3:4b).
cd "$(dirname "$0")" || exit 1
if ! curl -s http://127.0.0.1:11434/api/tags >/dev/null; then
  nohup ollama serve >/dev/null 2>&1 &
  sleep 3
fi
exec ./.venv/bin/python mod-src/launch_brutalplus_test.py --llm "$@"
