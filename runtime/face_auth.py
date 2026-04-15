#!/usr/bin/env python3
import sys
import time

import cv2


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

    timeout_sec = 12.0
    deadline = time.time() + timeout_sec
    detected_frames = 0
    required_frames = 5

    try:
        while time.time() < deadline:
            ok, frame = cap.read()
            if not ok:
                continue

            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            faces = cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(80, 80))

            if len(faces) > 0:
                detected_frames += 1
                if detected_frames >= required_frames:
                    print("Face recognized")
                    return 0
            else:
                # Avoid stale positives: require near-consecutive frames.
                detected_frames = max(0, detected_frames - 1)

        print("No stable face detected within timeout.", file=sys.stderr)
        return 4
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    raise SystemExit(main())
