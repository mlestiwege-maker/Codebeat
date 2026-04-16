# Codebeat

From-scratch C++ AI assistant project (tiny-first curriculum), with a Qt desktop shell and a gated splash authentication flow.

## Current stage (v0.1)

- ✅ C++20 project scaffold with Qt6 GUI
- ✅ ML engine core: tensor ops, autodiff tape, AdamW optimizer
- ✅ Byte-level tokenizer + transformer scaffolding
- ✅ Splash-screen authentication (wake word/passkey)
- ✅ In-app relock flow (return to first access screen)
- ✅ Working quick action buttons and conversational replies
- ✅ Real next-token training loop with gradient updates and loss trend output
- ✅ Training checkpoint save/load support for learned weights
- ✅ Runtime memory + tool integration (time/status/capabilities)

## GUI behavior

- App starts on splash access screen.
- Enter wake word/passkey to unlock and enter the assistant.
- Optional biometric button uses Linux face-auth in this order:
	1) `howdy test` (if installed)
	2) OpenCV owner-face verification fallback (`./face_auth.sh` -> `runtime/face_auth.py`)
- Splash screen also includes **Enroll Face** button (runs `./face_auth.sh --enroll`) so you can set owner profile without terminal.
- Face enrollment from splash now requires entering the passkey/password first.
- If `CODEBEAT_FACE_ONLY=1`, the splash auto-scans your face on startup and hides passkey unlock controls.
- Face verification now probes available camera indexes and auto-picks the camera with the strongest face signal.
- In main app, click **LOCK** (or type `lock`) to return to splash access screen.
- Black premium UI theme with neon accents.

## App/task control commands

Inside Codebeat chat, you can now run:

- `open chrome`
- `open vs code`
- `open terminal`
- `open <app_name>`
- `open https://<url>`
- `search <query>`
- `close <app_name>`
- `run <shell_command>`
- `execute <shell_command>`
- `voice status`
- `what can you control`

Voice control:

- Click `🎙 VOICE` in the main app and speak a command.
- Linux voice backend order:
	1) recorder: `arecord` -> `pw-record` -> `parec` -> `ffmpeg`
	2) transcriber: `whisper` CLI (or Python `openai-whisper` fallback)
- Voice capture script: `./voice_recognize.sh` (records 4s and transcribes).
- Voice decode now retries with normalized/boosted audio for low-volume microphone input.
- If voice fails, Codebeat now shows backend-specific error text in chat.
- Run `voice status` in chat to see detected recorder/ASR backends and active candidates.

Optional voice tuning env vars:

- `CODEBEAT_VOICE_SECONDS` (default: `10`)
- `CODEBEAT_PULSE_SOURCE` (default: `default`) to pick a specific Pulse/PipeWire input source
- `CODEBEAT_WHISPER_MODEL` (default: `tiny.en`) to choose whisper model (`tiny.en`, `base.en`, etc.)
- `CODEBEAT_CAMERA_INDEX` (default: `0`) to choose webcam index for face enroll/verify
- `CODEBEAT_FACE_THRESHOLD` (optional, default profile value `0.88`) to tune owner-match strictness

These values can be set in `.env` at project root (auto-loaded by `voice_recognize.sh` and `face_auth.sh`).

## Optional dependencies for local voice + face fallback

- Linux packages:
	- at least one recorder backend: `alsa-utils` (`arecord`) or PipeWire (`pw-record`) or PulseAudio (`parec`) or `ffmpeg`
- Python packages (in your project venv):
	- `openai-whisper`
	- `opencv-python`

## Enroll your face (owner-only unlock)

Before OpenCV fallback biometric unlock can verify identity, enroll your face once:

```bash
./face_auth.sh --enroll
```

- This saves an owner profile at `data/processed/face_profile.npz`.
- You can enroll up to **2 face profiles max** (Android-style alternate appearance concept).
- After `2/2` faces are enrolled, further enrollment is blocked and unlock continues by face scan only.
- Enrollment now calibrates owner threshold from your captured samples (`p10 - margin`) for better real-world reliability.
- Biometric fallback unlock now checks **match against your enrolled profile**, not just "any detected face".
- If no profile exists, face auth will instruct you to enroll first.
- If Howdy is installed but fails, Codebeat now automatically falls back to OpenCV verification.

## Troubleshooting voice/face quickly

- Voice says transcription failed:
	- Run `voice status` in chat and ensure at least one recorder + one ASR backend is available.
	- Try a different mic source in `.env` (for PipeWire/Pulse systems this is often the fix):
		- `CODEBEAT_PULSE_SOURCE=default` (or your explicit source name)
	- Keep a clear 4-6s command and avoid starting with silence.
- Face says profile missing:
	- Run `./face_auth.sh --enroll` once to create `data/processed/face_profile.npz`.
- Face cannot open camera:
	- Set `CODEBEAT_CAMERA_INDEX` in `.env` to `0`, `1`, or `2` based on your webcam.
- Face unlock feels slow:
	- Keep `CODEBEAT_FACE_ONLY=1` enabled so the app auto-starts face scanning.
- Face unlock rejects too often:
	- Improve lighting and re-enroll once.
	- Optionally lower `CODEBEAT_FACE_THRESHOLD` slightly (e.g. `0.86`) in `.env`.
	- When threshold is lower than `0.80`, Codebeat automatically requires more consecutive owner matches.

## How to run Codebeat

From the project root (`codebeat/`):

### 1) Build GUI + CLI

```bash
cmake -S . -B build_gui -DCODEBEAT_BUILD_GUI=ON
cmake --build build_gui -j
```

Or use one-command launchers from the project root:

```bash
./run_gui.sh
./train.sh
./repl.sh
```

### Start Codebeat automatically after login

Install autostart entry:

```bash
./install_autostart.sh
```

Remove autostart entry:

```bash
./uninstall_autostart.sh
```

### 2) Run desktop app

```bash
./build_gui/codebeat_gui
```

### 3) Run trainer

```bash
./build_gui/codebeat_train
```

### 4) Run REPL

```bash
./build_gui/codebeat_repl
```

### CLI only

```bash
cmake -S . -B build -DCODEBEAT_BUILD_GUI=OFF
cmake --build build -j
./build/codebeat_train
./build/codebeat_repl
```

### Training behavior

- `codebeat_train` now performs real next-token learning on `data/raw/corpus.txt`.
- It tokenizes text using byte-level encoding and trains over token pairs.
- It updates token embeddings and output projection weights using gradient descent.
- It reports per-epoch average loss and prints first/last loss trend.
- It saves checkpoints to `data/processed/codebeat_epoch_N.*` and `codebeat_latest.*`.

### With Qt GUI

```bash
cmake -S . -B build_gui -DCODEBEAT_BUILD_GUI=ON
cmake --build build_gui -j
./build_gui/codebeat_gui
```

## Project layout

```text
codebeat/
├─ engine/
├─ models/
├─ runtime/
├─ training/
├─ safety/
└─ data/
```
