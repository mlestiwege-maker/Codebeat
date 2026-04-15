#!/usr/bin/env bash
set -euo pipefail

# Captures a short microphone sample and transcribes it.
# Backend priority:
# 1) recorder: arecord -> pw-record -> parec -> ffmpeg
# 2) ASR: whisper CLI -> python whisper module (ffmpeg-free path)

TMP_WAV="$(mktemp /tmp/codebeat_voice_XXXXXX.wav)"
TMP_RAW="$(mktemp /tmp/codebeat_voice_raw_XXXXXX.pcm)"
TMP_DIR="$(mktemp -d /tmp/codebeat_whisper_XXXXXX)"
cleanup() {
  rm -f "$TMP_WAV" 2>/dev/null || true
  rm -f "$TMP_RAW" 2>/dev/null || true
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

RECORD_SECS="${CODEBEAT_VOICE_SECONDS:-4}"
PULSE_SOURCE="${CODEBEAT_PULSE_SOURCE:-default}"
WHISPER_MODEL="${CODEBEAT_WHISPER_MODEL:-tiny.en}"

raw_to_wav() {
  local raw_path="$1"
  local wav_path="$2"

  [[ -n "${PYTHON_BIN:-}" ]] || return 1
  "$PYTHON_BIN" - "$raw_path" "$wav_path" <<'PY'
import sys
import wave

raw_path, wav_path = sys.argv[1], sys.argv[2]
raw = open(raw_path, 'rb').read()
if not raw:
    raise SystemExit(1)

with wave.open(wav_path, 'wb') as wf:
    wf.setnchannels(1)
    wf.setsampwidth(2)
    wf.setframerate(16000)
    wf.writeframes(raw)
PY
}

record_audio() {
  # arecord (ALSA)
  if command -v arecord >/dev/null 2>&1; then
    arecord -q -f cd -t wav -d "$RECORD_SECS" "$TMP_WAV" && return 0
  fi

  # PipeWire native recorder
  if command -v pw-record >/dev/null 2>&1; then
    timeout "$((RECORD_SECS + 2))" pw-record --rate 16000 --channels 1 --format s16 --target "$PULSE_SOURCE" "$TMP_WAV" \
      && return 0
    # Retry without explicit target if default alias is unsupported on some systems.
    timeout "$((RECORD_SECS + 2))" pw-record --rate 16000 --channels 1 --format s16 "$TMP_WAV" && return 0
  fi

  # PulseAudio recorder (raw PCM -> wav via Python stdlib).
  if command -v parec >/dev/null 2>&1; then
    local bytes
    bytes=$((RECORD_SECS * 16000 * 2))
    if timeout "$((RECORD_SECS + 2))" parec --device "$PULSE_SOURCE" --format=s16le --rate=16000 --channels=1 --latency-msec=50 > "$TMP_RAW" 2>/dev/null; then
      if [[ -s "$TMP_RAW" ]] && raw_to_wav "$TMP_RAW" "$TMP_WAV" && [[ -s "$TMP_WAV" ]]; then
        return 0
      fi
    fi

    # fallback that force-stops parec after needed bytes
    if timeout "$((RECORD_SECS + 2))" bash -lc "parec --device '$PULSE_SOURCE' --format=s16le --rate=16000 --channels=1 --latency-msec=50 | head -c $bytes" > "$TMP_RAW" 2>/dev/null; then
      if [[ -s "$TMP_RAW" ]] && raw_to_wav "$TMP_RAW" "$TMP_WAV" && [[ -s "$TMP_WAV" ]]; then
        return 0
      fi
    fi
  fi

  # ffmpeg as last fallback.
  if command -v ffmpeg >/dev/null 2>&1; then
    ffmpeg -loglevel error -f pulse -i "$PULSE_SOURCE" -t "$RECORD_SECS" -ac 1 -ar 16000 "$TMP_WAV" && return 0
  fi

  return 1
}

if ! record_audio; then
  echo "No working microphone recorder found. Install/use one of: arecord, pw-record (PipeWire), parec (PulseAudio), or ffmpeg." >&2
  exit 2
fi

if command -v whisper >/dev/null 2>&1; then
  whisper "$TMP_WAV" --model "$WHISPER_MODEL" --language en --output_format txt --output_dir "$TMP_DIR" >/dev/null 2>&1 || {
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
    if "$PYTHON_BIN" - "$TMP_WAV" <<'PY'
import sys
import wave
from pathlib import Path

import numpy as np
import os
os.environ.setdefault("CUDA_VISIBLE_DEVICES", "")
import whisper

wav_path = Path(sys.argv[1])
if not wav_path.exists() or wav_path.stat().st_size == 0:
  raise SystemExit(1)

with wave.open(str(wav_path), 'rb') as wf:
  channels = wf.getnchannels()
  sample_width = wf.getsampwidth()
  framerate = wf.getframerate()
  nframes = wf.getnframes()
  raw = wf.readframes(nframes)

if sample_width != 2:
  raise SystemExit(2)

audio = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
if channels > 1:
  audio = audio.reshape(-1, channels).mean(axis=1)

target_rate = 16000
if framerate != target_rate and audio.size > 0:
  duration = audio.size / float(framerate)
  tgt_n = max(1, int(duration * target_rate))
  src_t = np.linspace(0.0, duration, num=audio.size, endpoint=False)
  tgt_t = np.linspace(0.0, duration, num=tgt_n, endpoint=False)
  audio = np.interp(tgt_t, src_t, audio).astype(np.float32)

audio = np.clip(audio, -1.0, 1.0)

model_name = os.environ.get("CODEBEAT_WHISPER_MODEL", "tiny.en")
model = whisper.load_model(model_name)
result = model.transcribe(audio, language="en", verbose=False)
text = (result.get("text") or "").strip()
if text:
  print(text)
  raise SystemExit(0)
raise SystemExit(4)
PY
    then
      exit 0
    fi
    code=$?
    if [[ $code -eq 4 ]]; then
      echo "Python whisper transcription produced no text." >&2
    else
      echo "Python whisper transcription failed." >&2
    fi
    exit 4
  fi
fi

echo "No ASR backend found. Install 'openai-whisper' in your Python environment or install whisper CLI." >&2
exit 5
