#!/usr/bin/env bash
# run_all.sh — Full test suite runner for Linux / WSL.
# Usage: ./tests/scripts/run_all.sh [BUILD_DIR] [WIN_BIN]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-build}"
WIN_BIN="${2:-}"        # optional path to Windows binary (from WSL)
BUILD_PATH="$ROOT/$BUILD_DIR"
BIN_PATH="$BUILD_PATH/bin/chess3d"
SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
FAILURES=()

# ── 1. Build ──────────────────────────────────────────────────────────────────
echo ""
echo "=== BUILD ==="
cmake --build "$BUILD_PATH"

# ── 2. Catch2 fast tests ──────────────────────────────────────────────────────
echo ""
echo "=== CATCH2 TESTS ==="
ctest --test-dir "$BUILD_PATH" --output-on-failure --label-exclude "slow" || FAILURES+=("catch2-fast")

# ── 3. Slow tests ─────────────────────────────────────────────────────────────
echo ""
echo "=== CATCH2 SLOW TESTS ==="
ctest --test-dir "$BUILD_PATH" --output-on-failure -L "slow" || FAILURES+=("catch2-slow")

# ── 4. Headless smoke test ────────────────────────────────────────────────────
echo ""
echo "=== HEADLESS SMOKE ==="
if output=$("$BIN_PATH" --auto --max-plies 200 --print-result 2>&1); then
    echo "$output"
    if echo "$output" | grep -q "^RESULT "; then
        echo "[OK] headless smoke"
    else
        echo "[WARN] headless smoke: no RESULT line"
        FAILURES+=("headless-smoke")
    fi
else
    echo "[FAIL] headless binary exited non-zero"
    FAILURES+=("headless-smoke")
fi

# ── 5. LAN cross-process scenarios ───────────────────────────────────────────
echo ""
echo "=== LAN SCENARIOS ==="
LAN_ARGS=("$SCRIPTS_DIR/run_all_lan.py" "--win-bin" "$BIN_PATH" "--max-plies" "300")
if [ -n "$WIN_BIN" ]; then LAN_ARGS+=("--lin-bin" "$WIN_BIN"); fi
python3 "${LAN_ARGS[@]}" || FAILURES+=("lan-scenarios")

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo "===================="
if [ ${#FAILURES[@]} -gt 0 ]; then
    echo "FAILED: ${FAILURES[*]}"
    exit 1
else
    echo "ALL TESTS PASSED"
    exit 0
fi
