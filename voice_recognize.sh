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

if [[ -f "$ROOT_DIR/.env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "$ROOT_DIR/.env"
  set +a
fi

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

RECORD_SECS="${CODEBEAT_VOICE_SECONDS:-5}"
PULSE_SOURCE="${CODEBEAT_PULSE_SOURCE:-default}"
WHISPER_MODEL="${CODEBEAT_WHISPER_MODEL:-tiny.en}"
VOICE_AUTO_SOURCE="${CODEBEAT_VOICE_AUTO_SOURCE:-1}"
VOICE_MIN_RMS="${CODEBEAT_VOICE_MIN_RMS:-0.0025}"
VOICE_DEBUG="${CODEBEAT_VOICE_DEBUG:-0}"
VOICE_SAVE_LAST="${CODEBEAT_VOICE_SAVE_LAST:-1}"

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

raw_rms() {
  local raw_path="$1"
  [[ -n "${PYTHON_BIN:-}" ]] || return 1
  "$PYTHON_BIN" - "$raw_path" <<'PY'
import sys
import numpy as np

raw_path = sys.argv[1]
try:
    b = open(raw_path, "rb").read()
except Exception:
    print("0.0")
    raise SystemExit(0)

if not b:
    print("0.0")
    raise SystemExit(0)

arr = np.frombuffer(b, dtype=np.int16).astype(np.float32) / 32768.0
if arr.size == 0:
    print("0.0")
    raise SystemExit(0)

rms = float(np.sqrt(np.mean(np.square(arr))))
print(f"{rms:.6f}")
PY
}

pick_best_pulse_source() {
  local fallback="$1"

  if [[ "$VOICE_AUTO_SOURCE" != "1" ]]; then
    echo "$fallback"
    return 0
  fi

  if ! command -v pactl >/dev/null 2>&1 || ! command -v parec >/dev/null 2>&1; then
    echo "$fallback"
    return 0
  fi

  local best_src="$fallback"
  local best_rms="0.0"
  local probe_secs=2
  local probe_bytes=$((probe_secs * 16000 * 2))

  local candidates
  candidates="default
$(pactl list short sources 2>/dev/null | awk '{print $2}' | grep -v '\\.monitor$' | head -n 8)"

  while IFS= read -r src; do
    [[ -n "${src:-}" ]] || continue

    if timeout "$((probe_secs + 2))" sh -c "parec --device '$src' --format=s16le --rate=16000 --channels=1 --latency-msec=50 | head -c $probe_bytes" > "$TMP_RAW" 2>/dev/null; then
      if [[ -s "$TMP_RAW" ]]; then
        local r
        r="$(raw_rms "$TMP_RAW" 2>/dev/null || echo "0.0")"
        if awk -v a="$r" -v b="$best_rms" 'BEGIN{exit !(a>b)}'; then
          best_rms="$r"
          best_src="$src"
        fi
      fi
    fi
  done <<< "$candidates"

  if awk -v a="$best_rms" -v b="$VOICE_MIN_RMS" 'BEGIN{exit !(a>=b)}'; then
    echo "$best_src"
  else
    echo "$fallback"
  fi
}

record_with_parec() {
  local source="$1"
  local secs="$2"
  local bytes
  bytes=$((secs * 16000 * 2))

  if timeout "$((secs + 2))" parec --device "$source" --format=s16le --rate=16000 --channels=1 --latency-msec=50 > "$TMP_RAW" 2>/dev/null; then
    if [[ -s "$TMP_RAW" ]] && raw_to_wav "$TMP_RAW" "$TMP_WAV" && [[ -s "$TMP_WAV" ]]; then
      return 0
    fi
  fi

  if timeout "$((secs + 2))" parec --format=s16le --rate=16000 --channels=1 --latency-msec=50 > "$TMP_RAW" 2>/dev/null; then
    if [[ -s "$TMP_RAW" ]] && raw_to_wav "$TMP_RAW" "$TMP_WAV" && [[ -s "$TMP_WAV" ]]; then
      return 0
    fi
  fi

  if timeout "$((secs + 2))" sh -c "parec --device '$source' --format=s16le --rate=16000 --channels=1 --latency-msec=50 | head -c $bytes" > "$TMP_RAW" 2>/dev/null; then
    if [[ -s "$TMP_RAW" ]] && raw_to_wav "$TMP_RAW" "$TMP_WAV" && [[ -s "$TMP_WAV" ]]; then
      return 0
    fi
  fi

  if timeout "$((secs + 2))" sh -c "parec --format=s16le --rate=16000 --channels=1 --latency-msec=50 | head -c $bytes" > "$TMP_RAW" 2>/dev/null; then
    if [[ -s "$TMP_RAW" ]] && raw_to_wav "$TMP_RAW" "$TMP_WAV" && [[ -s "$TMP_WAV" ]]; then
      return 0
    fi
  fi

  return 1
}

record_audio() {
  local source
  source="$(pick_best_pulse_source "$PULSE_SOURCE")"

  if [[ "$VOICE_DEBUG" == "1" ]]; then
    echo "[voice] selected_source=$source" >&2
  fi

  # arecord (ALSA)
  if command -v arecord >/dev/null 2>&1; then
    arecord -q -f cd -t wav -d "$RECORD_SECS" "$TMP_WAV" && return 0
  fi

  # PipeWire native recorder
  if command -v pw-record >/dev/null 2>&1; then
    timeout "$((RECORD_SECS + 2))" pw-record --rate 16000 --channels 1 --format s16 --target "$source" "$TMP_WAV" \
      && return 0
    # Retry without explicit target if default alias is unsupported on some systems.
    timeout "$((RECORD_SECS + 2))" pw-record --rate 16000 --channels 1 --format s16 "$TMP_WAV" && return 0
  fi

  # PulseAudio recorder (raw PCM -> wav via Python stdlib).
  if command -v parec >/dev/null 2>&1; then
    record_with_parec "$source" "$RECORD_SECS" && return 0
  fi

  # ffmpeg as last fallback.
  if command -v ffmpeg >/dev/null 2>&1; then
    ffmpeg -loglevel error -f pulse -i "$source" -t "$RECORD_SECS" -ac 1 -ar 16000 "$TMP_WAV" && return 0
  fi

  return 1
}

if ! record_audio; then
  echo "No working microphone recorder found. Install/use one of: arecord, pw-record (PipeWire), parec (PulseAudio), or ffmpeg." >&2
  exit 2
fi

if [[ "$VOICE_SAVE_LAST" == "1" ]]; then
  mkdir -p "$ROOT_DIR/data/processed" >/dev/null 2>&1 || true
  cp "$TMP_WAV" "$ROOT_DIR/data/processed/last_voice.wav" >/dev/null 2>&1 || true
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
import traceback
import warnings
from pathlib import Path

import numpy as np
import os
os.environ.setdefault("CUDA_VISIBLE_DEVICES", "")
import whisper

warnings.filterwarnings("ignore", message=".*CUDA initialization.*")
warnings.filterwarnings("ignore", message=".*FP16 is not supported on CPU.*")

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

# Trim long leading/trailing silence while preserving some context.
if audio.size > 0:
  mask = np.abs(audio) > 0.004
  if np.any(mask):
    idx = np.flatnonzero(mask)
    pad = int(0.15 * target_rate)
    start = max(0, int(idx[0]) - pad)
    end = min(audio.size, int(idx[-1]) + pad)
    audio = audio[start:end]

model_name = os.environ.get("CODEBEAT_WHISPER_MODEL", "tiny.en")

def _decode(model, arr, *, force_english):
  kwargs = {
      "verbose": False,
      "condition_on_previous_text": False,
      "fp16": False,
      "temperature": 0,
        "no_speech_threshold": 0.70,
      "compression_ratio_threshold": 2.8,
        "logprob_threshold": -2.0,
        "initial_prompt": "desktop voice commands: open chrome, open vs code, open terminal, search, run, close, status, lock",
  }
  if force_english:
    kwargs["language"] = "en"

  result = model.transcribe(arr, **kwargs)
  return (result.get("text") or "").strip()

rms = float(np.sqrt(np.mean(np.square(audio)))) if audio.size else 0.0

try:
  model = whisper.load_model(model_name)

  text = _decode(model, audio, force_english=True)

  # Second pass: normalize/boost low-volume captures and allow auto language.
  if not text:
    work = np.copy(audio)
    if rms > 0:
      target_rms = 0.08
      gain = min(12.0, max(1.0, target_rms / max(rms, 1e-6)))
      work = np.clip(work * gain, -1.0, 1.0)
    text = _decode(model, work, force_english=False)

  # Third pass: relaxed decode for difficult/noisy mic captures.
  if not text:
    work = np.copy(audio)
    if rms > 0:
      target_rms = 0.10
      gain = min(18.0, max(1.0, target_rms / max(rms, 1e-6)))
      work = np.clip(work * gain, -1.0, 1.0)
    kwargs = {
      "verbose": False,
      "condition_on_previous_text": False,
      "fp16": False,
      "temperature": (0.0, 0.2, 0.4),
      "no_speech_threshold": 1.0,
      "compression_ratio_threshold": 3.0,
      "logprob_threshold": -3.0,
      "initial_prompt": "voice command",
    }
    result = model.transcribe(work, **kwargs)
    text = (result.get("text") or "").strip()

  if text:
    print(text)
    raise SystemExit(0)
  peak = float(np.max(np.abs(audio))) if audio.size else 0.0
  print(f"No speech text detected in audio (rms={rms:.4f}, peak={peak:.4f}).", file=sys.stderr)
  raise SystemExit(4)
except SystemExit:
  raise
except Exception as e:
  print(f"Python whisper error: {e}", file=sys.stderr)
  traceback.print_exc(file=sys.stderr)
  raise SystemExit(6)
PY
    then
      exit 0
    else
      code=$?
    fi

    if [[ $code -eq 4 ]]; then
      echo "Python whisper transcription produced no text." >&2
    elif [[ $code -eq 6 ]]; then
      echo "Python whisper runtime error (see details above)." >&2
    else
      echo "Python whisper transcription failed." >&2
    fi
    exit 4
  fi
fi

echo "No ASR backend found. Install 'openai-whisper' in your Python environment or install whisper CLI." >&2
exit 5
