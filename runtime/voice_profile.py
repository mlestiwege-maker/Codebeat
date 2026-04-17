#!/usr/bin/env python3
import argparse
import math
import wave
from pathlib import Path

import numpy as np


def load_wav(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as wf:
        channels = wf.getnchannels()
        sample_width = wf.getsampwidth()
        sample_rate = wf.getframerate()
        nframes = wf.getnframes()
        raw = wf.readframes(nframes)

    if sample_width != 2:
        raise ValueError("Only 16-bit PCM wav is supported")

    audio = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
    if channels > 1:
        audio = audio.reshape(-1, channels).mean(axis=1)

    return audio, sample_rate


def resample_linear(audio: np.ndarray, src_rate: int, dst_rate: int = 16000) -> np.ndarray:
    if src_rate == dst_rate or audio.size == 0:
        return audio

    duration = audio.size / float(src_rate)
    tgt_n = max(1, int(duration * dst_rate))
    src_t = np.linspace(0.0, duration, num=audio.size, endpoint=False)
    tgt_t = np.linspace(0.0, duration, num=tgt_n, endpoint=False)
    return np.interp(tgt_t, src_t, audio).astype(np.float32)


def trim_silence(audio: np.ndarray, sr: int) -> np.ndarray:
    if audio.size == 0:
        return audio

    mask = np.abs(audio) > 0.01
    if not np.any(mask):
        return audio

    idx = np.flatnonzero(mask)
    pad = int(0.10 * sr)
    start = max(0, int(idx[0]) - pad)
    end = min(audio.size, int(idx[-1]) + pad)
    return audio[start:end]


def spectral_band_features(audio: np.ndarray, n_bands: int = 32) -> np.ndarray:
    if audio.size == 0:
        return np.zeros((n_bands,), dtype=np.float32)

    # Simple magnitude spectrum profile across coarse frequency bands.
    window = np.hanning(audio.size).astype(np.float32)
    spec = np.abs(np.fft.rfft(audio * window))
    if spec.size <= 1:
        return np.zeros((n_bands,), dtype=np.float32)

    # Ignore DC bin; use normalized log-energy distribution.
    spec = spec[1:]
    eps = 1e-8
    edges = np.linspace(0, spec.size, num=n_bands + 1, dtype=np.int32)
    bands = []
    for i in range(n_bands):
        a, b = edges[i], edges[i + 1]
        if b <= a:
            bands.append(0.0)
            continue
        band = spec[a:b]
        energy = float(np.mean(band * band))
        bands.append(math.log(energy + eps))

    v = np.array(bands, dtype=np.float32)
    v -= float(np.mean(v))
    denom = float(np.linalg.norm(v)) + 1e-8
    return v / denom


def extract_voice_feature(audio: np.ndarray, sr: int) -> np.ndarray:
    audio = resample_linear(audio, sr, 16000)
    sr = 16000
    audio = trim_silence(audio, sr)

    if audio.size == 0:
        return np.zeros((36,), dtype=np.float32)

    # keep roughly first 2.5s of active voice for consistency
    max_len = int(2.5 * sr)
    if audio.size > max_len:
        audio = audio[:max_len]

    bands = spectral_band_features(audio, n_bands=32)

    rms = float(np.sqrt(np.mean(np.square(audio)))) if audio.size else 0.0
    zcr = float(np.mean(np.abs(np.diff(np.signbit(audio).astype(np.float32))))) if audio.size > 1 else 0.0
    peak = float(np.max(np.abs(audio))) if audio.size else 0.0
    mean = float(np.mean(audio)) if audio.size else 0.0

    tail = np.array([rms, zcr, peak, mean], dtype=np.float32)
    # normalize tail scale
    tail = np.clip(tail, -1.0, 1.0)

    feat = np.concatenate([bands, tail], axis=0).astype(np.float32)
    norm = float(np.linalg.norm(feat)) + 1e-8
    return feat / norm


def cosine_distance(a: np.ndarray, b: np.ndarray) -> float:
    aa = float(np.linalg.norm(a)) + 1e-8
    bb = float(np.linalg.norm(b)) + 1e-8
    sim = float(np.dot(a, b) / (aa * bb))
    return 1.0 - sim


def enroll(profile_path: Path, wav_path: Path, threshold: float) -> int:
    audio, sr = load_wav(wav_path)
    feat = extract_voice_feature(audio, sr)

    profile_path.parent.mkdir(parents=True, exist_ok=True)
    np.savez(profile_path, feature=feat, threshold=np.array([threshold], dtype=np.float32))
    print(f"ENROLL_OK threshold={threshold:.4f}")
    return 0


def check(profile_path: Path, wav_path: Path, threshold_override: float | None = None) -> int:
    if not profile_path.exists():
        print("CHECK_ERROR missing_profile")
        return 3

    p = np.load(profile_path)
    owner = p["feature"].astype(np.float32)
    stored_threshold = float(p["threshold"][0]) if "threshold" in p else 0.35
    threshold = float(threshold_override) if threshold_override is not None else stored_threshold

    audio, sr = load_wav(wav_path)
    feat = extract_voice_feature(audio, sr)

    score = cosine_distance(owner, feat)
    label = "owner" if score <= threshold else "unknown"
    print(f"CHECK label={label} score={score:.5f} threshold={threshold:.5f}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Codebeat voice owner profile helper")
    ap.add_argument("--profile", default="data/processed/voice_owner_profile.npz")
    ap.add_argument("--wav", required=True)
    ap.add_argument("--enroll", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--threshold", type=float, default=0.33)
    ap.add_argument("--threshold-override", type=float, default=None)
    args = ap.parse_args()

    profile_path = Path(args.profile)
    wav_path = Path(args.wav)

    if not wav_path.exists():
        print("CHECK_ERROR missing_wav")
        return 2

    if args.enroll:
        return enroll(profile_path, wav_path, args.threshold)
    if args.check:
        return check(profile_path, wav_path, args.threshold_override)

    print("CHECK_ERROR specify --enroll or --check")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
