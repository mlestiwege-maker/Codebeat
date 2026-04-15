#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY_FACE_SCRIPT="$ROOT_DIR/runtime/face_auth.py"

pick_python() {
  local candidates=(
    "$ROOT_DIR/.venv/bin/python"
    "$ROOT_DIR/../.venv/bin/python"
    "$(command -v python3 || true)"
  )
  for p in "${candidates[@]}"; do
    if [[ -n "${p:-}" && -x "$p" ]]; then
      echo "$p"
      return 0
    fi
  done
  return 1
}

if command -v howdy >/dev/null 2>&1; then
  howdy test >/dev/null 2>&1 && exit 0
  echo "Howdy is installed but biometric verification failed." >&2
  exit 10
fi

PYTHON_BIN="$(pick_python || true)"
if [[ -z "${PYTHON_BIN:-}" ]]; then
  echo "No Python interpreter found for fallback face authentication." >&2
  exit 11
fi

if [[ ! -f "$PY_FACE_SCRIPT" ]]; then
  echo "Face authentication script not found: $PY_FACE_SCRIPT" >&2
  exit 12
fi

if ! "$PYTHON_BIN" -c "import cv2" >/dev/null 2>&1; then
  echo "OpenCV not installed for Python fallback. Install package: opencv-python" >&2
  exit 13
fi

"$PYTHON_BIN" "$PY_FACE_SCRIPT"