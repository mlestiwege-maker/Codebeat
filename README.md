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
- Optional biometric button uses Linux face-auth hook (`howdy test`) if available.
- In main app, click **LOCK** (or type `lock`) to return to splash access screen.
- Black premium UI theme with neon accents.

## App/task control commands

Inside Codebeat chat, you can now run:

- `open chrome`
- `open vs code`
- `open terminal`
- `open <app_name>`
- `run <shell_command>`
- `what can you control`

Voice control:

- Click `🎙 VOICE` in the main app and speak a command.
- Linux dependency for built-in voice path: `arecord` + `whisper` CLI.
- Voice capture script: `./voice_recognize.sh` (records 4s and transcribes).

## Build

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
