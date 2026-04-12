#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AUTOSTART_DIR="$HOME/.config/autostart"
DESKTOP_FILE="$AUTOSTART_DIR/codebeat.desktop"

mkdir -p "$AUTOSTART_DIR"

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Codebeat
Comment=Start Codebeat assistant at login
Exec=$ROOT_DIR/run_gui.sh
Terminal=false
X-GNOME-Autostart-enabled=true
Categories=Utility;
EOF

chmod +x "$ROOT_DIR/run_gui.sh"

echo "Autostart installed: $DESKTOP_FILE"
