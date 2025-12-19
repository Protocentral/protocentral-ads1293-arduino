#!/bin/bash
# Compile and upload example 3 (5-lead ECG OpenView) to Arduino Uno R4 Minima
#
# Usage: ./upload_uno_r4.sh [port]
#   port: Optional serial port (auto-detected if not specified)
#
# Requirements: arduino-cli must be installed and configured

set -e

FQBN="arduino:renesas_uno:minima"
SKETCH="examples/03-5-lead-ECG-openview"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$SCRIPT_DIR"

# Install core if not present
if ! arduino-cli core list | grep -q "arduino:renesas_uno"; then
    echo "Installing Arduino Uno R4 core..."
    arduino-cli core install arduino:renesas_uno
fi

# Compile
echo "Compiling $SKETCH for $FQBN..."
arduino-cli compile --fqbn "$FQBN" "$SKETCH"

# Determine port
if [ -n "$1" ]; then
    PORT="$1"
else
    PORT=$(arduino-cli board list | grep -i "minima\|renesas" | awk '{print $1}' | head -1)
    if [ -z "$PORT" ]; then
        echo "Error: No Arduino Uno R4 Minima detected. Specify port manually."
        echo "Usage: $0 [port]"
        exit 1
    fi
fi

echo "Uploading to $PORT..."
arduino-cli upload --fqbn "$FQBN" --port "$PORT" "$SKETCH"

echo "Done!"
