#!/usr/bin/env bash
# Build and run ballcore.  Usage: scripts/run.sh [camera_device] [port]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$ROOT/core" -B "$ROOT/core/build" >/dev/null
cmake --build "$ROOT/core/build" -j4
exec "$ROOT/core/build/ballcore" "$@"
