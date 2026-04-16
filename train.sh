#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build_gui"

if [[ -x "$ROOT_DIR/build_corpus.sh" ]]; then
	"$ROOT_DIR/build_corpus.sh"
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCODEBEAT_BUILD_GUI=ON
cmake --build "$BUILD_DIR" -j

exec "$BUILD_DIR/codebeat_train" "$@"
