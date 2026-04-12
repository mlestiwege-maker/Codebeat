#!/usr/bin/env bash
set -euo pipefail

DESKTOP_FILE="$HOME/.config/autostart/codebeat.desktop"
if [[ -f "$DESKTOP_FILE" ]]; then
  rm -f "$DESKTOP_FILE"
  echo "Autostart removed: $DESKTOP_FILE"
else
  echo "Autostart entry not found: $DESKTOP_FILE"
fi
