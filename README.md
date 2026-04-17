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
- Left tactical panel with **collapsible sections** for clean UI:
	- **Quick Control** (always expanded): open chrome, open vs code, search tutorial, status
	- **Voice Access** (▶ collapsed by default): click the arrow to expand and access voice identity, audit controls, standby tuning, and enrollment
		- `voice identity status`, `voice audit status`, `voice audit summary`
		- `open voice log`, `open logs folder`, `open latest export`, `copy latest export path`, `list exports`
		- `voice audit clear`, `voice audit export`
		- `voice standby on/off`, `voice standby status`, `voice standby sensitivity up/down`, `voice standby window <seconds>`
		- `stop listening`, `go to sleep`, `quiet mode` (interrupt voice capture mid-stream)
		- `voice enroll owner`, `voice enroll trusted`
	- **System Info** (▶ collapsed by default): click the arrow to expand and access battery, processes, screenshot quick-access buttons
- **Voice Role Badge** (live indicator on tactical panel) displays real-time status:
	- Green ✓ `VOICE ROLE: owner` (owner voice detected)
	- Yellow ⚡ `VOICE ROLE: trusted` (trusted voice detected)
	- Gray ◯ `VOICE ROLE: unknown` (no match)
	- Dark gray ◯ `VOICE ROLE: disabled` (voice identity off)
	- Auto-updates whenever voice is captured and evaluated
	- **First-time setup:** Run `voice enroll owner` to create your owner profile, then captures will show your identity
	- If badge stays `unknown`, check that a voice profile exists: `ls -la data/processed/voice_owner_profile.npz`
- Black premium UI theme with neon accents.

## App/task control commands

Inside Codebeat chat, you can now run:

- `open chrome`
- `open vs code`
- `open terminal`
- `open downloads`
- `create folder project-notes`
- `open <app_name>`
- `open https://<url>`
- `search <query>`
- `google <query>` / `look up <query>`
- `open docs for <topic>`
- `close browser` / `close chrome`
- `close <app_name>`
- `run <shell_command>`
- `execute <shell_command>`
- `auth face`
- `auth passkey <your-passkey>`
- `voice status`
- `voice style status`
- `voice style <default|calm|warm|clear|broadcast>`
- `voice audit status`
- `voice audit summary`
- `voice audit open`
- `voice audit folder`
- `voice audit open latest`
- `voice audit copy latest`
- `voice audit list exports`
- `voice audit clear`
- `voice audit export`
- `voice standby on` / `voice standby off`
- `voice standby status`
- `voice standby sensitivity up` / `voice standby sensitivity down`
- `voice standby window 6`
- `voice identity status`
- `ollama status`
- `local ai <prompt>`
- `voice enroll owner`
- `voice enroll trusted`
- `check battery` / `battery status`
- `show running processes`
- `take screenshot`
- `refresh app` / `refresh ui`
- `refresh auto on` / `refresh auto off` / `refresh auto status`
- `code status`
- `code diff summary`
- `code recent commits`
- `open project file <relative/path>`
- `create feature branch <name>`
- `plugin status` / `plugin reload`
- `what can you control`
- `learn: <fact>`
- `knowledge status`
- `plan <goal>`
- `brainstorm <topic>` / `ideas <topic>`
- `compare <option A> vs <option B>`
- `rewrite: <style>::<text>`
- `summarize: <text>`
- `mode concise` / `mode detailed` / `mode status`
- natural language examples:
	- `I want to write some code` → opens VS Code
	- `please create a folder called project-notes` → creates a local folder in your home directory
	- `show voice status` → checks the current voice mode and capture state
	- `voice style warm` → applies a warmer, more natural speaking preset
	- `voice style status` → shows active style and TTS pacing values
	- `turn auto refresh on` → enables automatic UI refreshes
	- `open my downloads folder` → opens `~/Downloads`
	- `make standby more sensitive` → shortens the wake-command window
	- `make standby less sensitive` → lengthens the wake-command window
	- `show standby status` → prints the current standby state and wake window
	- `increase the wake window` → makes standby less sensitive
	- `show running processes` → runs `ps aux`
	- `check battery` → checks battery state
	- `google qt signals slots` → opens a web search
	- `open docs for cmake` → searches documentation
	- `close browser` → closes Chrome/Chromium if running
	- `ollama status` → shows whether local Ollama support is enabled
	- `local ai explain recursion` → asks the local Ollama model a question
	- `code status` → shows current git branch + working tree summary
	- `code diff summary` → shows file-level diff stats
	- `code recent commits` → shows the latest 5 commit subjects
	- `open project file README.md` → opens a repository file in your system editor
	- `create feature branch feature/voice-coding` → creates/switches to a new branch
	- `plugin status` → shows plugin file path + loaded plugin count
	- `plugin reload` → reloads command plugins from disk
	- `volume up` / `volume down` / `mute` / `unmute` → controls audio
	- `take screenshot` → captures the screen

### Safety + critical-auth guards

Tool-use commands like `run` and `execute` are protected by a **confirmation gate**:

- When safety mode is enabled (`CODEBEAT_SAFETY_MODE=1` in `.env`), risky commands ask for user confirmation first.
- Codebeat will respond with: "Execute: <cmd>?\n\nReply 'yes' to confirm or 'no' to cancel."
- Reply with `yes`, `y`, or `confirm` to execute. Reply with `no`, `n`, or `cancel` to abort.
- This prevents accidental script execution and maintains audit clarity (you always see _what_ Codebeat intends to run).
- Critical operations (`sudo`, `shutdown`, `reboot`, destructive commands, etc.) can require a second authentication step.
- For critical tasks, Codebeat asks you to authenticate using:
  - `auth face` (runs local face verification), or
  - `auth passkey <your-passkey>` (if configured in `.env`).
- You can disable confirmation per-command via `.env`:
	- `CODEBEAT_CONFIRM_RUN=0` to skip confirmation for `run <cmd>`
	- `CODEBEAT_CONFIRM_EXECUTE=0` to skip confirmation for `execute <cmd>`
	- `CODEBEAT_SAFETY_MODE=0` to disable all safety guards globally (not recommended).

Voice control:

- Click `🎙 VOICE` in the main app and speak a command.
- Codebeat writes down the transcript it heard, runs the command, and speaks replies aloud using a TTS backend chain:
	1) `piper` (best quality, if configured with a model)
	2) `spd-say`
	3) `espeak-ng`
	4) `espeak`
- Voice mode badge now reflects live state: `READY`, `STANDBY`, `LISTENING`, and `SPEAKING`.
- Auto-refresh badge now shows live state: `OFF` or `ON (<interval>s)`.
- A compact **Heard queue** strip shows the last 3 normalized transcripts for quick verification before/after execution.
- Continuous standby mode (wake + command loop):
	- Type `voice standby on` to enable always-listening standby.
	- Say any wake alias (from `CODEBEAT_WAKE_ALIASES`), for example: `hey codebeat`, `hello friend`, `dad's son`.
	- Codebeat responds with **Listening...** and accepts your next command.
	- Say `stop listening`, `sleep mode`, `go to sleep`, or run `voice standby off` to pause standby mode.
- Linux voice backend order:
	1) recorder: `arecord` -> `pw-record` -> `parec` -> `ffmpeg`
	2) transcriber: `whisper` CLI (or Python `openai-whisper` fallback)
- Voice capture script: `./voice_recognize.sh` (records 4s and transcribes).
- Voice decode now retries with normalized/boosted audio for low-volume microphone input.
- If voice fails, Codebeat now shows backend-specific error text in chat.
- Run `voice status` in chat to see detected recorder/ASR backends and active candidates.
- `voice status` now also shows detected TTS backends + current TTS config.
- Run `voice audit status` to check voice audit log health and last recorded entry.
- Run `voice audit summary` to see a compact report with counts, last role/score, and paths.
- Run `voice audit open` to open the audit log file in your system viewer/editor.
- Run `voice audit folder` to open the folder containing audit snapshots and logs.
- Run `voice audit open latest` to open the most recent exported snapshot directly.
- Run `voice audit copy latest` to copy the most recent exported snapshot path to your clipboard.
- Run `voice audit list exports` to print the recent exported snapshots in chat.
- Run `voice audit clear` to clear the audit log after confirmation.
- Run `voice audit export` to create a timestamped snapshot copy of the audit log.
- Run `voice identity status` to see whether the current speaker matches owner profile.
- Run `voice enroll owner` once to create/update local owner voice profile.
- Run `voice enroll trusted` to enroll one trusted non-owner voice profile.
- Voice-triggered turns are logged to `data/logs/voice_commands.log` with timestamp, role, score, command, and outcome.
- Runtime replies now also consult `data/raw/corpus.txt` for lightweight local knowledge grounding.

Optional safety policy tuning env vars:

- `CODEBEAT_SAFETY_MODE` (default: `1`) Enable (1) or disable (0) all safety guards
- `CODEBEAT_CONFIRM_RUN` (default: `1`) Require confirmation before executing `run <cmd>` (1=yes, 0=no)
- `CODEBEAT_CONFIRM_EXECUTE` (default: `1`) Require confirmation before executing `execute <cmd>` (1=yes, 0=no)
- `CODEBEAT_REQUIRE_AUTH_CRITICAL` (default: `1`) Require step-up authentication for critical commands
- `CODEBEAT_CRITICAL_PASSKEY` (optional) Passkey value used by `auth passkey <value>`
- `CODEBEAT_CRITICAL_AUTH_TTL` (default: `120`) Seconds critical auth remains valid after success
- `CODEBEAT_WAKE_ALIASES` (default includes `codebeat,hey codebeat,hello codebeat,hello friend,dad's son`) comma-separated wake aliases for voice normalization
- `CODEBEAT_WAKE_WINDOW_SECONDS` (default: `8`) seconds after wake phrase to accept the next command in standby mode
- `CODEBEAT_VOICE_OWNER_MODE` (default: `1`) enable owner-voice recognition checks
- `CODEBEAT_VOICE_OWNER_FOR_CRITICAL` (default: `1`) require owner voice for critical **voice-triggered** actions

Note: if an alias contains an apostrophe (example `dad's son`), wrap the whole `.env` value in double quotes to avoid shell parse errors.

When confirmation is required, Codebeat will ask: "Execute: <cmd>?\n\nReply 'yes' to confirm or 'no' to cancel."

Optional voice tuning env vars:

- `CODEBEAT_VOICE_SECONDS` (default: `5`)
- `CODEBEAT_PULSE_SOURCE` (default: `default`) to pick a specific Pulse/PipeWire input source
- `CODEBEAT_WHISPER_MODEL` (default: `tiny.en`) to choose whisper model (`tiny.en`, `base.en`, etc.)
- `CODEBEAT_VOICE_AUTO_SOURCE` (default: `1`) to auto-select the Pulse source with strongest voice signal
- `CODEBEAT_VOICE_MIN_RMS` (default: `0.0025`) minimum signal floor used during source auto-selection
- `CODEBEAT_VOICE_DEBUG` (default: `0`) set to `1` to print selected mic source and extra voice diagnostics to stderr
- `CODEBEAT_VOICE_SAVE_LAST` (default: `1`) save the latest capture to `data/processed/last_voice.wav` for voice identity checks
- `CODEBEAT_VOICE_OUTPUT` (default: `1`) speak assistant replies aloud with `spd-say` when available; set to `0` for silent mode
- `CODEBEAT_TTS_ENGINE` (default: `auto`) choose TTS backend order target: `auto`, `piper`, `spd-say`, `espeak-ng`, `espeak`
- `CODEBEAT_TTS_STYLE` (default: `warm`) voice preset: `default`, `calm`, `warm`, `clear`, `broadcast`
- `CODEBEAT_TTS_VOICE` (default: `en-us`) voice id for `spd-say` / `espeak(-ng)`
- `CODEBEAT_TTS_RATE` (default: `165`) speech rate (range `80-260`)
- `CODEBEAT_TTS_PITCH` (default: `45`) speech pitch (range `0-99`, espeak backends)
- `CODEBEAT_TTS_PIPER_MODEL` (optional) path to local Piper `.onnx` model for natural human-like voice
- `CODEBEAT_TTS_PIPER_SPEAKER` (default: `0`) Piper speaker id (`-1` uses model default)
- `CODEBEAT_TTS_MAX_CHARS` (default: `280`) max assistant reply length sent to TTS before truncation
- `CODEBEAT_TTS_CHUNK_CHARS` (default: `150`) target size for sentence chunking to improve cadence
- `CODEBEAT_TTS_PAUSE_MS` (default: `90`) short pause between spoken chunks for more natural pacing
- `CODEBEAT_CAMERA_INDEX` (default: `0`) to choose webcam index for face enroll/verify
- `CODEBEAT_FACE_THRESHOLD` (optional, default profile value `0.88`) to tune owner-match strictness

For the most natural voice quality, install Piper and set `CODEBEAT_TTS_PIPER_MODEL` to a valid voice model path.

Local AI / Ollama env vars:

- `CODEBEAT_OLLAMA_ENABLED` (default: `0`) enable local Ollama-backed replies
- `CODEBEAT_OLLAMA_BASE_URL` (default: `http://localhost:11434`) Ollama API base URL
- `CODEBEAT_OLLAMA_MODEL` (default: `llama3.2`) model to use for local replies

## Plugin system (JSON command plugins)

Codebeat can load custom command plugins from:

- `data/raw/plugins/commands.json`

Built-in plugin controls:

- `plugin status` → shows where plugins are loaded from and how many are active
- `plugin reload` → reloads plugin definitions without restarting the app

Supported plugin actions:

- `reply` → return static text to chat
- `open_url` → open a URL via `xdg-open`
- `run` → run a shell command (requires `allow_run=true`; still subject to safety confirmations)

Example plugin entry:

```json
{
	"trigger": "plugin hello",
	"action": "reply",
	"value": "Hello from plugin",
	"enabled": true
}
```

These values can be set in `.env` at project root (auto-loaded by `voice_recognize.sh` and `face_auth.sh`).

## Knowledge pack + corpus workflow

- Add/edit topic files under `data/raw/knowledge/*.txt` (one fact/rule per line).
- You can also teach from chat in GUI: `learn: <fact>` (saved to `data/raw/knowledge/user_facts.txt` and appended to `data/raw/corpus.txt`).
- Rebuild merged training corpus:

```bash
./build_corpus.sh
```

- `./train.sh` now automatically runs corpus rebuild before training.
- Runtime grounded replies read from `data/raw/corpus.txt`, so corpus updates affect both training and in-app knowledge retrieval.

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
	- Keep `CODEBEAT_VOICE_AUTO_SOURCE=1` so Codebeat auto-picks the strongest mic source.
	- If mic is too quiet, lower `CODEBEAT_VOICE_MIN_RMS` slightly (for example `0.0018`).
	- Set `CODEBEAT_VOICE_DEBUG=1` to inspect selected mic source in terminal diagnostics.
	- Keep a clear 4-6s command and avoid starting with silence.
	- Start directly with command words (e.g. `open terminal`) to avoid filler-only transcripts.
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

Trainer options:

```bash
./build_gui/codebeat_train --epochs 20 --lr 0.005 --corpus data/raw/corpus.txt
```

Or via launcher:

```bash
./train.sh --epochs 20 --lr 0.005
```

Environment alternatives:

- `CODEBEAT_TRAIN_EPOCHS`
- `CODEBEAT_TRAIN_LR`
- `CODEBEAT_TRAIN_CORPUS`

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
- Training is configurable through CLI flags/env vars (epochs, learning rate, corpus path).
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
