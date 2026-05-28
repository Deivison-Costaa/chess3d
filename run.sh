#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXE="$SCRIPT_DIR/build/bin/chess3d"

if [ ! -f "$EXE" ]; then
    echo "[ERRO] Binario nao encontrado: $EXE"
    echo "Execute o build primeiro:"
    echo "  cmake -B build -S . --preset linux-gcc"
    echo "  cmake --build build"
    exit 1
fi

exec "$EXE" "$@"
