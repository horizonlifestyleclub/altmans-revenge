#!/bin/sh
# Brutal Plus vs Classic Brutal (the original AI); you are allied with both and only watch.
cd "$(dirname "$0")" || exit 1
exec ./.venv/bin/python mod-src/launch_brutalplus_test.py --versus "$@"
