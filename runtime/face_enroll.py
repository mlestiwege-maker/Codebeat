#!/usr/bin/env python3
import json
import sys
import time
from pathlib import Path

import cv2
import numpy as np


def _profile_path() -> Path:
    root = Path(__file__).resolve().parents[1]
    out = root / "data" / "processed"
    out.mkdir(parents=True, exist_ok=True)
    return out / "face_profile.npz"


def _extract_descriptor(gray: np.ndarray, cascade: cv2.CascadeClassifier) -> np.ndarray | None:
    faces = cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=6, minSize=(90, 90))
    if len(faces) == 0:
        return None

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


def main() -> int:
    cascade_path = cv2.data.haarcascades + "haarcascade_frontalface_default.xml"
    cascade = cv2.CascadeClassifier(cascade_path)
    if cascade.empty():
        print("Could not load OpenCV face cascade.", file=sys.stderr)
        return 2

    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("Could not open camera device 0.", file=sys.stderr)
        return 3

    target_samples = 24
    timeout_sec = 25.0
    deadline = time.time() + timeout_sec
    descriptors: list[np.ndarray] = []

    print("Face enrollment started. Keep only your face in frame and look straight at the camera.")

    try:
        while time.time() < deadline and len(descriptors) < target_samples:
            ok, frame = cap.read()
            if not ok:
                continue

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            desc = _extract_descriptor(gray, cascade)
            if desc is None:
                continue

            # Keep diverse but stable samples.
            if descriptors:
                sim = float(np.dot(descriptors[-1], desc))
                if sim > 0.995:
                    continue

            descriptors.append(desc)
            if len(descriptors) % 4 == 0:
                print(f"Captured {len(descriptors)}/{target_samples} samples...")

        if len(descriptors) < 12:
            print(
                f"Enrollment failed: only captured {len(descriptors)} good samples. Improve lighting and retry.",
                file=sys.stderr,
            )
            return 4

        mean_desc = np.mean(np.stack(descriptors, axis=0), axis=0).astype(np.float32)
        mean_desc /= np.linalg.norm(mean_desc) + 1e-8

        profile_path = _profile_path()
        meta = {
            "created_at": int(time.time()),
            "samples": len(descriptors),
            "threshold": 0.88,
            "method": "opencv_haar_hog_cosine",
        }
        np.savez_compressed(profile_path, descriptor=mean_desc, meta=json.dumps(meta))
        print(f"Enrollment complete. Owner face profile saved to: {profile_path}")
        return 0
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    raise SystemExit(main())
