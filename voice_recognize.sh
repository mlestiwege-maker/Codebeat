#!/usr/bin/env bash
set -euo pipefail

# Captures a short microphone sample and transcribes it.
# Backend priority: whisper CLI (if installed).

TMP_WAV="$(mktemp /tmp/codebeat_voice_XXXXXX.wav)"
TMP_DIR="$(mktemp -d /tmp/codebeat_whisper_XXXXXX)"
cleanup() {
  rm -f "$TMP_WAV" 2>/dev/null || true
  rm -rf "$TMP_DIR" 2>/dev/null || true
}
trap cleanup EXIT

if ! command -v arecord >/dev/null 2>&1; then
  exit 2
fi

# Record 4 seconds from default mic
arecord -q -f cd -t wav -d 4 "$TMP_WAV" || exit 3

if command -v whisper >/dev/null 2>&1; then
  whisper "$TMP_WAV" --model tiny --language en --output_format txt --output_dir "$TMP_DIR" >/dev/null 2>&1 || exit 4
  BASE_NAME="$(basename "${TMP_WAV%.*}")"
  TXT_FILE="$TMP_DIR/$BASE_NAME.txt"
  if [[ -f "$TXT_FILE" ]]; then
    cat "$TXT_FILE"
    exit 0
  fi
fi

exit 5
