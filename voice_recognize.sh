#!/usr/bin/env bash
set -euo pipefail

# Captures a short microphone sample and transcribes it.
# Backend priority:
# 1) recorder: arecord -> ffmpeg
# 2) ASR: whisper CLI -> python whisper module

TMP_WAV="$(mktemp /tmp/codebeat_voice_XXXXXX.wav)"
TMP_DIR="$(mktemp -d /tmp/codebeat_whisper_XXXXXX)"
cleanup() {
  rm -f "$TMP_WAV" 2>/dev/null || true
  rm -rf "$TMP_DIR" 2>/dev/null || true
}
trap cleanup EXIT

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

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

PYTHON_BIN="$(pick_python || true)"

# Record 4 seconds from default mic.
if command -v arecord >/dev/null 2>&1; then
  arecord -q -f cd -t wav -d 4 "$TMP_WAV" || {
    echo "Microphone capture failed via arecord." >&2
    exit 3
  }
elif command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -loglevel error -f pulse -i default -t 4 "$TMP_WAV" || {
    echo "Microphone capture failed via ffmpeg/pulse." >&2
    exit 3
  }
else
  echo "No microphone recorder found. Install 'alsa-utils' (arecord) or 'ffmpeg'." >&2
  exit 2
fi

if command -v whisper >/dev/null 2>&1; then
  whisper "$TMP_WAV" --model tiny --language en --output_format txt --output_dir "$TMP_DIR" >/dev/null 2>&1 || {
    echo "Whisper CLI transcription failed." >&2
    exit 4
  }
  BASE_NAME="$(basename "${TMP_WAV%.*}")"
  TXT_FILE="$TMP_DIR/$BASE_NAME.txt"
  if [[ -f "$TXT_FILE" ]]; then
    cat "$TXT_FILE"
    exit 0
  fi
fi

if [[ -n "${PYTHON_BIN:-}" ]]; then
  if "$PYTHON_BIN" -c "import whisper" >/dev/null 2>&1; then
    "$PYTHON_BIN" - <<'PY' "$TMP_WAV"
import sys
from pathlib import Path

import whisper

wav = Path(sys.argv[1])
model = whisper.load_model("tiny")
result = model.transcribe(str(wav), language="en")
text = (result.get("text") or "").strip()
if text:
    print(text)
    raise SystemExit(0)
raise SystemExit(1)
PY
    status=$?
    if [[ $status -eq 0 ]]; then
      exit 0
    fi
    echo "Python whisper transcription produced no text." >&2
    exit 4
  fi
fi

echo "No ASR backend found. Install 'openai-whisper' in your Python environment or install whisper CLI." >&2
exit 5
