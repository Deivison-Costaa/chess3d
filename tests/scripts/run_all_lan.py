#!/usr/bin/env python3
"""
run_all_lan.py — Orchestrates all LAN cross-process scenarios.

Scenarios:
  1. Win  ↔ Win   : win_bin (host) vs win_bin (client)
  2. Lin  ↔ Lin   : lin_bin (host) vs lin_bin (client) — requires WSL
  3. Win  ↔ Lin   : win_bin (host) vs lin_bin (client) — requires WSL
  4. Lin  ↔ Win   : lin_bin (host) vs win_bin (client) — requires WSL

Pass --win-bin and optionally --lin-bin; scenarios requiring missing binaries are skipped.

Usage:
  python run_all_lan.py --win-bin build/bin/chess3d.exe
                        [--lin-bin /mnt/c/.../chess3d-linux/build/bin/chess3d]
                        [--base-port 5100] [--max-plies 300] [--timeout 120]
"""
import argparse
import os
import subprocess
import sys


def run_scenario(name: str, bin_a: str, bin_b: str, port: int, max_plies: int, timeout: int) -> bool:
    print(f"\n{'='*60}")
    print(f"SCENARIO: {name}  (port {port})")
    print(f"{'='*60}")

    script = os.path.join(os.path.dirname(__file__), "run_lan_match.py")
    cmd = [
        sys.executable, script,
        "--bin-a", bin_a,
        "--bin-b", bin_b,
        "--port", str(port),
        "--max-plies", str(max_plies),
        "--timeout", str(timeout),
    ]
    result = subprocess.run(cmd)
    if result.returncode == 0:
        print(f"[PASS] {name}")
        return True
    else:
        print(f"[FAIL] {name}", file=sys.stderr)
        return False


def main():
    p = argparse.ArgumentParser(description="Run all LAN interop scenarios")
    p.add_argument("--win-bin",   required=True, help="Windows chess3d binary")
    p.add_argument("--lin-bin",   default=None,   help="Linux chess3d binary (WSL path)")
    p.add_argument("--base-port", type=int, default=5100, dest="base_port")
    p.add_argument("--max-plies", type=int, default=300,  dest="max_plies")
    p.add_argument("--timeout",   type=int, default=120)
    args = p.parse_args()

    failures = []
    port = args.base_port

    # ── Scenario 1: Win ↔ Win ──────────────────────────────────────────────
    ok = run_scenario("Win↔Win", args.win_bin, args.win_bin,
                      port, args.max_plies, args.timeout)
    if not ok: failures.append("Win↔Win")
    port += 1

    if args.lin_bin:
        if not os.path.exists(args.lin_bin) and not args.lin_bin.startswith("/mnt/"):
            print(f"[SKIP] lin-bin not found: {args.lin_bin}")
        else:
            # ── Scenario 2: Lin ↔ Lin ──────────────────────────────────────
            ok = run_scenario("Lin↔Lin", args.lin_bin, args.lin_bin,
                              port, args.max_plies, args.timeout)
            if not ok: failures.append("Lin↔Lin")
            port += 1

            # ── Scenario 3: Win ↔ Lin ──────────────────────────────────────
            ok = run_scenario("Win↔Lin (Win=host)", args.win_bin, args.lin_bin,
                              port, args.max_plies, args.timeout)
            if not ok: failures.append("Win↔Lin")
            port += 1

            # ── Scenario 4: Lin ↔ Win ──────────────────────────────────────
            ok = run_scenario("Lin↔Win (Lin=host)", args.lin_bin, args.win_bin,
                              port, args.max_plies, args.timeout)
            if not ok: failures.append("Lin↔Win")
    else:
        print("\n[INFO] --lin-bin not provided; skipping Linux interop scenarios")

    print(f"\n{'='*60}")
    if failures:
        print(f"FAILED scenarios: {', '.join(failures)}", file=sys.stderr)
        sys.exit(1)
    else:
        print(f"ALL SCENARIOS PASSED")
        sys.exit(0)


if __name__ == "__main__":
    main()
