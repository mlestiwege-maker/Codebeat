#!/usr/bin/env python3
import json
import os
import sys
import time
from pathlib import Path

os.environ.setdefault("OPENCV_LOG_LEVEL", "SILENT")

import cv2
import numpy as np


def _profile_path() -> Path:
    root = Path(__file__).resolve().parents[1]
    out = root / "data" / "processed"
    out.mkdir(parents=True, exist_ok=True)
    return out / "face_profile.npz"


def _detect_faces(gray: np.ndarray, cascade: cv2.CascadeClassifier):
    # Multi-pass detect to be more tolerant to lighting/angle variation.
    passes = [
        (1.10, 6, (90, 90)),
        (1.08, 5, (72, 72)),
        (1.06, 4, (56, 56)),
    ]

    for scale, neighbors, min_size in passes:
        faces = cascade.detectMultiScale(
            gray,
            scaleFactor=scale,
            minNeighbors=neighbors,
            minSize=min_size,
        )
        if len(faces) > 0:
            return faces

    # Try contrast-enhanced image as fallback.
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    enhanced = clahe.apply(gray)
    for scale, neighbors, min_size in passes:
        faces = cascade.detectMultiScale(
            enhanced,
            scaleFactor=scale,
            minNeighbors=max(3, neighbors - 1),
            minSize=min_size,
        )
        if len(faces) > 0:
            return faces

    return []


def _extract_descriptor(gray: np.ndarray, cascade: cv2.CascadeClassifier) -> np.ndarray | None:
    faces = _detect_faces(gray, cascade)
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


def _open_best_camera(camera_candidates: list[int], cascade: cv2.CascadeClassifier):
    best = None
    best_idx = None
    best_score = -1.0
    best_hits = 0

    for idx in camera_candidates:
        if not Path(f"/dev/video{idx}").exists():
            continue
        cap = cv2.VideoCapture(idx)
        if not cap.isOpened():
            cap.release()
            continue

        # Warm-up and quick probe for face-signal quality.
        hits = 0
        sharp_sum = 0.0
        sampled = 0
        for _ in range(12):
            ok, frame = cap.read()
            if not ok:
                continue
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            sampled += 1
            if len(_detect_faces(gray, cascade)) > 0:
                hits += 1
            sharp_sum += float(cv2.Laplacian(gray, cv2.CV_64F).var())

        avg_sharp = (sharp_sum / sampled) if sampled > 0 else 0.0
        score = hits * 1000.0 + avg_sharp

        if score > best_score:
            if best is not None:
                best.release()
            best = cap
            best_idx = idx
            best_score = score
            best_hits = hits
        else:
            cap.release()

    return best, best_idx, best_hits


def main() -> int:
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

    cap, opened_idx, probe_hits = _open_best_camera(camera_candidates, cascade)

    if cap is None:
        tried = ", ".join(str(i) for i in camera_candidates)
        print(f"Could not open camera device. Tried indexes: {tried}", file=sys.stderr)
        return 3

    target_samples = 16
    timeout_sec = 30.0 if probe_hits < 3 else 18.0
    min_required_samples = 6 if probe_hits < 3 else 8
    deadline = time.time() + timeout_sec
    descriptors: list[np.ndarray] = []

    print(
        f"Face enrollment started on camera {opened_idx} (probe_face_hits={probe_hits}/12). "
        "Keep only your face in frame and look straight at the camera."
    )

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

        if len(descriptors) < min_required_samples:
            hint = (
                "No face was detected in preview. Move into frame and face the camera directly."
                if probe_hits == 0
                else "Improve lighting and keep your face centered."
            )
            print(
                f"Enrollment failed: only captured {len(descriptors)} good samples. {hint}",
                file=sys.stderr,
            )
            return 4

        mean_desc = np.mean(np.stack(descriptors, axis=0), axis=0).astype(np.float32)
        mean_desc /= np.linalg.norm(mean_desc) + 1e-8

        sims = [float(np.dot(d, mean_desc)) for d in descriptors]
        sims_arr = np.array(sims, dtype=np.float32)
        p10 = float(np.percentile(sims_arr, 10)) if sims_arr.size else 0.88
        calibrated_threshold = max(0.72, min(0.90, p10 - 0.10))

        profile_path = _profile_path()
        meta = {
            "created_at": int(time.time()),
            "samples": len(descriptors),
            "threshold": calibrated_threshold,
            "method": "opencv_haar_hog_cosine",
        }
        np.savez_compressed(profile_path, descriptor=mean_desc, meta=json.dumps(meta))
        print(
            f"Enrollment complete. Owner face profile saved to: {profile_path} "
            f"(threshold={calibrated_threshold:.3f}, p10={p10:.3f})"
        )
        return 0
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    raise SystemExit(main())
