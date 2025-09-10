#!/usr/bin/env bash
set -euo pipefail

# Combined flash helper for protocentral-ads1293-arduino
# Supports compiling and optionally uploading/monitoring examples for multiple targets.
#
# Usage examples:
#   ./scripts/flash.sh --example 1 --board esp32 --upload --port /dev/cu.usbserial-0001
#   ./scripts/flash.sh --example 3 --fqbn "esp32:esp32:esp32" --compile-only
#   ./scripts/flash.sh --example all --fqbn "arduino:avr:uno" --upload --port /dev/ttyUSB0

PROG_NAME=$(basename "$0")

DEFAULT_FQBN_ESP32="esp32:esp32:esp32"
DEFAULT_FQBN_UNO="arduino:avr:uno"

EX1_PATH="examples/Example1-3leadECGstream-arduino-plotter"
EX2_PATH="examples/Example2-5leadECGstream-arduino-plotter"
EX3_PATH="examples/Example3-5leadECGstream-openview"

EXAMPLES_AVAILABLE=("1" "2" "3")

show_help() {
  cat <<EOF
Usage: $PROG_NAME [OPTIONS]

Options:
  --example N|all     Example to act on: 1,2,3 or all (default: 1)
  --board NAME        Shorthand board: esp32 or uno (sets a sensible default FQBN)
  --fqbn FQBN         Fully-qualified board name passed to arduino-cli (overrides --board)
  --upload            After compile, upload to device (requires --port)
  --port PORT         Serial port to use for upload/monitor (e.g. /dev/cu.usbserial-xxxx)
  --monitor           Open serial monitor after upload (uses arduino-cli monitor)
  --compile-only      Only compile (do not upload)
  --clean             Run a "compile --clean" before building
  -h, --help          Show this message

Examples:
  $PROG_NAME --example 3 --board esp32 --upload --port /dev/cu.usbserial-0001
  $PROG_NAME --example all --fqbn "arduino:avr:uno" --compile-only
EOF
}

if [ $# -eq 0 ]; then
  show_help
  exit 1
fi

EXAMPLE="1"
FQBN=""
UPLOAD=false
PORT=""
MONITOR=false
COMPILE_ONLY=false
CLEAN=false

while [ $# -gt 0 ]; do
  case "$1" in
    --example)
      EXAMPLE="$2"; shift 2;;
    --board)
      case "$2" in
        esp32) FQBN="$DEFAULT_FQBN_ESP32"; shift 2;;
        uno) FQBN="$DEFAULT_FQBN_UNO"; shift 2;;
        *) echo "Unknown board shorthand: $2"; exit 2;;
      esac;;
    --fqbn)
      FQBN="$2"; shift 2;;
    --upload)
      UPLOAD=true; shift 1;;
    --port)
      PORT="$2"; shift 2;;
    --monitor)
      MONITOR=true; shift 1;;
    --compile-only)
      COMPILE_ONLY=true; shift 1;;
    --clean)
      CLEAN=true; shift 1;;
    -h|--help)
      show_help; exit 0;;
    *) echo "Unknown option: $1"; show_help; exit 2;;
  esac
done

run_example() {
  local exnum="$1"
  local path=""
  case "$exnum" in
    1) path="$EX1_PATH";;
    2) path="$EX2_PATH";;
    3) path="$EX3_PATH";;
    *) echo "Unsupported example: $exnum"; return 2;;
  esac

  # Choose a sensible default FQBN if none provided
  if [ -z "$FQBN" ]; then
    FQBN="$DEFAULT_FQBN_ESP32"
    echo "No --fqbn provided; defaulting to $FQBN (override with --fqbn)"
  fi

  echo "==> Compiling example${exnum} at $path for $FQBN"
  if $CLEAN; then
    arduino-cli compile --fqbn "$FQBN" --clean "$path"
  fi
  arduino-cli compile --fqbn "$FQBN" "$path"

  if $COMPILE_ONLY; then
    return 0
  fi

  if $UPLOAD; then
    if [ -z "$PORT" ]; then
      echo "--upload requested but no --port provided"; return 2
    fi
    echo "==> Uploading to $PORT"
    arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$path"
  fi

  if $MONITOR; then
    if [ -z "$PORT" ]; then
      echo "--monitor requested but no --port provided"; return 2
    fi
    echo "==> Opening serial monitor on $PORT"
    # If you want to change the baud, set MON_BAUD env var before calling this script
    MON_BAUD=${MON_BAUD:-57600}
    arduino-cli monitor -p "$PORT" -b "$FQBN" -c "$MON_BAUD"
  fi
}

if [ "$EXAMPLE" = "all" ]; then
  for e in "${EXAMPLES_AVAILABLE[@]}"; do
    run_example "$e"
  done
else
  run_example "$EXAMPLE"
fi

echo "Done."
