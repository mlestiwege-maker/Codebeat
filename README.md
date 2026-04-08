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

## GUI behavior

- App starts on splash access screen.
- Enter wake word/passkey to unlock and enter the assistant.
- In main app, click **LOCK** (or type `lock`) to return to splash access screen.
- Black premium UI theme with neon accents.

## Build

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
