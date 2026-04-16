#!/usr/bin/env python3
import json
import os
import sys
import time
from pathlib import Path

import cv2
import numpy as np


def _profile_path() -> Path:
    root = Path(__file__).resolve().parents[1]
    return root / "data" / "processed" / "face_profile.npz"


def _load_profile(path: Path) -> tuple[np.ndarray, dict] | tuple[None, None]:
    if not path.exists():
        return None, None
    data = np.load(str(path), allow_pickle=True)
    descriptor = data.get("descriptor")
    if descriptor is None:
        return None, None
    descriptor = np.asarray(descriptor, dtype=np.float32).reshape(-1)
    meta = {}
    meta_blob = data.get("meta")
    if meta_blob is not None:
        try:
            meta = json.loads(str(meta_blob.item()))
        except Exception:
            meta = {}
    return descriptor, meta


def _extract_descriptor(gray: np.ndarray, cascade: cv2.CascadeClassifier) -> np.ndarray | None:
    faces = cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=6, minSize=(90, 90))
    if len(faces) == 0:
        return None

    # Use the largest face in frame to reduce background/false-positive noise.
    x, y, w, h = max(faces, key=lambda f: int(f[2] * f[3]))
    face = gray[y : y + h, x : x + w]
    if face.size == 0:
        return None

    face = cv2.resize(face, (64, 128), interpolation=cv2.INTER_LINEAR)
    face = cv2.equalizeHist(face)

    hog = cv2.HOGDescriptor(
        _winSize=(64, 128),
        _blockSize=(16, 16),
        _blockStride=(8, 8),
        _cellSize=(8, 8),
        _nbins=9,
    )
    vec = hog.compute(face)
    if vec is None:
        return None

    desc = vec.reshape(-1).astype(np.float32)
    norm = np.linalg.norm(desc)
    if norm <= 1e-8:
        return None
    return desc / norm


def _cosine_similarity(a: np.ndarray, b: np.ndarray) -> float:
    if a.shape != b.shape:
        return -1.0
    return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-8))


def main() -> int:
    profile_file = _profile_path()
    enrolled_desc, meta = _load_profile(profile_file)
    if enrolled_desc is None:
        print(
            "No enrolled owner face profile found. Run: ./face_auth.sh --enroll",
            file=sys.stderr,
        )
        return 14

    threshold = float(meta.get("threshold", 0.88)) if isinstance(meta, dict) else 0.88

    cascade_path = cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
    cascade = cv2.CascadeClassifier(cascade_path)
    if cascade.empty():
        print("Could not load OpenCV face cascade.", file=sys.stderr)
        return 2

    requested_idx = os.environ.get("CODEBEAT_CAMERA_INDEX", "").strip()
    camera_candidates: list[int] = []
    if requested_idx:
        try:
            camera_candidates.append(int(requested_idx))
        except ValueError:
            pass
    for idx in (0, 1, 2):
        if idx not in camera_candidates:
            camera_candidates.append(idx)

    cap = None
    opened_idx = None
    for idx in camera_candidates:
        candidate = cv2.VideoCapture(idx)
        if candidate.isOpened():
            cap = candidate
            opened_idx = idx
            break
        candidate.release()

    if cap is None:
        tried = ", ".join(str(i) for i in camera_candidates)
        print(f"Could not open camera device. Tried indexes: {tried}", file=sys.stderr)
        return 3

    timeout_sec = 8.0
    deadline = time.time() + timeout_sec
    matched_frames = 0
    required_matches = 2
    best_score = -1.0

    try:
        while time.time() < deadline:
            ok, frame = cap.read()
            if not ok:
                continue

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            live_desc = _extract_descriptor(gray, cascade)
            if live_desc is None:
                matched_frames = max(0, matched_frames - 1)
                continue

            score = _cosine_similarity(enrolled_desc, live_desc)
            if score > best_score:
                best_score = score

            if score >= threshold:
                matched_frames += 1
                if matched_frames >= required_matches:
                    print(f"Owner face recognized (camera {opened_idx})")
                    return 0
            else:
                # Decay quickly so non-owner or unstable frames cannot pass.
                matched_frames = max(0, matched_frames - 2)

        print(
            f"Face did not match enrolled owner profile (best={best_score:.3f}, need>={threshold:.3f}).",
            file=sys.stderr,
        )
        return 4
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    raise SystemExit(main())
