#!/usr/bin/env zsh
# Convenience script to compile, upload Example1 and open a serial monitor
# Usage: ./flash_example1.sh [PORT]
# If PORT is not provided the script will try to detect an Arduino UNO R4 Minima
# connected to the machine, or fall back to the first /dev/cu.usbmodem* device.

set -euo pipefail

# Determine script directory and repo root. Support both:
# - repo-root/flash_example1.sh
# - repo-root/scripts/flash_example1.sh
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ "$(basename "$SCRIPT_DIR")" == "scripts" ]]; then
  REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  REPO_ROOT="$SCRIPT_DIR"
fi
EXAMPLE_PATH="$REPO_ROOT/examples/Example1-3leadECGstream-arduino-plotter"
FQBN="arduino:renesas_uno:minima"
BAUD=115200

# If the first argument is provided, use it as the port
PORT="${1:-}"

if [[ -z "$PORT" ]]; then
  # Try to find an attached UNO R4 Minima via arduino-cli board list
  PORT=$(arduino-cli board list 2>/dev/null | awk '/Arduino UNO R4 Minima/ {print $1; exit}') || true
fi

if [[ -z "$PORT" ]]; then
  # Fallback: pick the first typical usbmodem device on macOS
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n1 || true)
fi

if [[ -z "$PORT" ]]; then
  echo "No serial port detected. Pass the port as the first argument, e.g."
  echo "  $0 /dev/cu.usbmodem11201"
  exit 1
fi

echo "Using port: $PORT"

echo "Compiling example..."
arduino-cli compile --verify --fqbn "$FQBN" "$EXAMPLE_PATH"

echo "Uploading to device..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$EXAMPLE_PATH"

echo "Opening serial monitor at $BAUD baud. Press Ctrl-C to exit."
# newer arduino-cli versions expect the baud via --config
arduino-cli monitor -p "$PORT" --config "$BAUD"
