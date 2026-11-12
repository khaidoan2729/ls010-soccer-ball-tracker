#!/usr/bin/env bash
# One-time setup on Raspberry Pi OS 64-bit (Bookworm).
set -euo pipefail
sudo apt update
sudo apt install -y build-essential cmake git libopencv-dev nlohmann-json3-dev \
  libcpp-httplib-dev v4l-utils
echo "Done. Plug in the webcam, then run: scripts/run.sh"
