#!/usr/bin/env zsh
set -euo pipefail

# flash_example3_r4.sh
# Convenience script: compile, upload and open serial monitor for Example3
# tailored for Arduino UNO R4 (Minima).
# Usage: ./flash_example3_r4.sh [PORT]
# If PORT is omitted the script will try to auto-detect an UNO R4 Minima.

SCRIPT_DIR="$(cd "$(dirname "${0}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"
EXAMPLE_DIR="$REPO_ROOT/examples/Example3-5leadECGstream-openview"
FQBN="${FQBN:-arduino:renesas_uno:minima}"
BAUD="${BAUD:-115200}"

# find port: first CLI arg wins
if [[ -n "${1:-}" ]]; then
  PORT="$1"
else
  PORT=""
  if command -v arduino-cli >/dev/null 2>&1; then
    # Look specifically for an attached UNO R4 Minima
    PORT="$(arduino-cli board list 2>/dev/null | awk '/Arduino UNO R4 Minima/ {print $1; exit}' || true)"
  fi
  # fallback to common /dev entries on macOS, prefer usbmodem devices
  if [[ -z "$PORT" ]]; then
    PORT="$(ls /dev/cu.usbmodem* /dev/cu.usbserial* /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n1 || true)"
  fi
fi

if [[ -z "$PORT" ]]; then
  echo "ERROR: No serial port detected. Pass port as first arg, e.g. ./flash_example3_r4.sh /dev/cu.usbmodem11201"
  exit 1
fi

echo "Using port: $PORT"

echo "==> Compiling Example3 ($EXAMPLE_DIR) for $FQBN"
arduino-cli compile --fqbn "$FQBN" "$EXAMPLE_DIR"

echo "==> Uploading to $PORT"
# If the port is temporarily busy (e.g. serial monitor open), wait up to 10s
if command -v lsof >/dev/null 2>&1; then
  retries=0
  while lsof "$PORT" >/dev/null 2>&1 && (( retries < 10 )); do
    echo "Port $PORT appears busy. Waiting... ($((retries+1))/10)"
    sleep 1
    retries=$((retries+1))
  done
  if lsof "$PORT" >/dev/null 2>&1; then
    echo "Warning: port $PORT still busy. Upload may fail."
  fi
fi

arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$EXAMPLE_DIR"

# small settle
sleep 1

echo "==> Opening serial monitor at $BAUD (Ctrl-C to exit)"
# prefer arduino-cli monitor if available
if command -v arduino-cli >/dev/null 2>&1; then
  arduino-cli monitor -p "$PORT" --config "$BAUD"
else
  echo "arduino-cli not found; falling back to 'screen'"
  screen "$PORT" "$BAUD"
fi
