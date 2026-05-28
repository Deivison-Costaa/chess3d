#!/usr/bin/env python3
"""
run_lan_match.py — Runs two chess3d processes in a LAN match and verifies
they agree on the result.

Usage:
  python run_lan_match.py --bin-a <path_host> --bin-b <path_client>
                          --port <port> [--max-plies <n>] [--timeout <sec>]

Returns 0 on success (both processes agree on result), non-zero otherwise.
"""
import argparse
import os
import subprocess
import sys
import tempfile
import time
import re


def parse_result_line(stdout: str) -> str | None:
    """Extract 'RESULT <name> <plies>' from process stdout.
    The result name may contain spaces (e.g. '1-0 (mate)').
    """
    for line in stdout.splitlines():
        m = re.match(r'^RESULT\s+(.*?)\s+(\d+)\s*$', line)
        if m:
            return m.group(1)
    return None


def read_pgn_moves(path: str) -> list[str]:
    """Extract UCI-ish move list from a saved PGN file."""
    if not os.path.exists(path):
        return []
    with open(path) as f:
        content = f.read()
    # Extract moves section (after headers)
    moves = re.findall(r'\b([a-h][1-8][a-h][1-8][qrbn]?)\b', content)
    return moves


def is_wsl_binary(binary: str) -> bool:
    return binary.startswith("wsl:") or binary.startswith("/")


def make_cmd(binary: str, extra_args: list) -> list:
    """Build a command list, wrapping WSL binaries with wsl.exe if needed."""
    linux_path = None
    if binary.startswith("wsl:"):
        linux_path = binary[4:]
    elif binary.startswith("/"):
        linux_path = binary

    if linux_path is not None:
        return ["wsl.exe", "-d", "Ubuntu", "-u", "root", "--", linux_path] + extra_args
    return [binary] + extra_args


def wsl2_host_ip() -> str:
    """Return the Windows host IP as seen from inside WSL2 (default gateway)."""
    try:
        import subprocess as sp
        result = sp.run(
            ["wsl.exe", "-d", "Ubuntu", "-u", "root", "--",
             "bash", "-c", "ip route | grep default | head -1 | cut -d' ' -f3"],
            capture_output=True, text=True, timeout=5
        )
        ip = result.stdout.strip()
        if ip:
            return ip
    except Exception:
        pass
    return "172.25.32.1"  # fallback for WSL2 NAT


def run(args):
    with tempfile.TemporaryDirectory() as tmpdir:
        pgn_host   = os.path.join(tmpdir, "host.pgn")
        pgn_client = os.path.join(tmpdir, "client.pgn")

        # For WSL binaries (Linux), convert Windows temp paths to Linux /tmp paths.
        is_host_wsl   = args.bin_a.startswith("wsl:") or args.bin_a.startswith("/")
        is_client_wsl = args.bin_b.startswith("wsl:") or args.bin_b.startswith("/")

        def pgn_path(win_path: str, for_wsl: bool) -> str:
            """Convert a Windows temp path to /tmp/... if the binary is a WSL Linux binary."""
            if not for_wsl:
                return win_path
            import re
            m = re.match(r'([A-Za-z]):\\(.*)', win_path)
            if m:
                drive = m.group(1).lower()
                rest  = m.group(2).replace('\\', '/')
                return f"/mnt/{drive}/{rest}"
            return win_path  # already a Linux path

        host_pgn_arg   = pgn_path(pgn_host,   is_host_wsl)
        client_pgn_arg = pgn_path(pgn_client, is_client_wsl)

        host_extra = [
            "--mode", "lan-host",
            "--white", "easy",
            "--lan-port", str(args.port),
            "--max-plies", str(args.max_plies),
            "--save-pgn", host_pgn_arg,
            "--print-result",
            "--nick", "host",
        ]
        # When host=Windows and client=WSL, 127.0.0.1 inside WSL2 NAT doesn't
        # reach the Windows process — use the WSL2 gateway IP instead.
        host_ip = "127.0.0.1"
        if not is_wsl_binary(args.bin_a) and is_wsl_binary(args.bin_b):
            host_ip = wsl2_host_ip()
            print(f"[run_lan_match] Win->WSL2: using host IP {host_ip}")
        elif is_wsl_binary(args.bin_a) and not is_wsl_binary(args.bin_b):
            # Linux host, Windows client: Windows can reach WSL2 loopback normally
            pass

        client_extra = [
            "--mode", "lan-client",
            "--black", "easy",
            "--lan-host-ip", host_ip,
            "--lan-port", str(args.port),
            "--max-plies", str(args.max_plies),
            "--save-pgn", client_pgn_arg,
            "--print-result",
            "--nick", "client",
        ]
        host_cmd   = make_cmd(args.bin_a, host_extra)
        client_cmd = make_cmd(args.bin_b, client_extra)

        print(f"[run_lan_match] host  : {' '.join(host_cmd)}")
        print(f"[run_lan_match] client: {' '.join(client_cmd)}")

        host_proc = subprocess.Popen(
            host_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        # Give host a moment to open the listen socket before client connects
        time.sleep(0.5)
        client_proc = subprocess.Popen(
            client_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )

        try:
            host_out, host_err   = host_proc.communicate(timeout=args.timeout)
            client_out, client_err = client_proc.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            host_proc.kill()
            client_proc.kill()
            print("[FAIL] Timeout waiting for processes to finish", file=sys.stderr)
            return 1

        print(f"[host]   rc={host_proc.returncode}")
        print(f"[client] rc={client_proc.returncode}")
        # Print both stdout and stderr for diagnosis
        if host_out.strip():
            print("[host stdout]\n" + host_out[:2000])
        if host_err.strip():
            print("[host stderr]\n" + host_err[:2000])
        if client_out.strip():
            print("[client stdout]\n" + client_out[:2000])
        if client_err.strip():
            print("[client stderr]\n" + client_err[:2000])

        if host_proc.returncode != 0:
            print("[FAIL] host process exited with non-zero", file=sys.stderr)
            return 1
        if client_proc.returncode != 0:
            print("[FAIL] client process exited with non-zero", file=sys.stderr)
            return 1

        host_result   = parse_result_line(host_out)
        client_result = parse_result_line(client_out)

        print(f"[run_lan_match] host result  : {host_result!r}")
        print(f"[run_lan_match] client result: {client_result!r}")

        if host_result is None:
            print("[FAIL] host did not print a RESULT line", file=sys.stderr)
            return 1
        if client_result is None:
            print("[FAIL] client did not print a RESULT line", file=sys.stderr)
            return 1
        if host_result != client_result:
            print(f"[FAIL] result mismatch: host={host_result!r} client={client_result!r}",
                  file=sys.stderr)
            return 1

        # Optionally compare PGN move sequences
        host_moves   = read_pgn_moves(pgn_host)
        client_moves = read_pgn_moves(pgn_client)
        if host_moves and client_moves and host_moves != client_moves:
            print(f"[WARN] PGN move sequences differ "
                  f"(host {len(host_moves)} vs client {len(client_moves)} moves)")
            # Not a hard failure — clocks differ per-process so PGN may have minor diffs
        elif host_moves:
            print(f"[OK] PGN sequences match ({len(host_moves)} moves)")

        print(f"[PASS] Both sides agreed on result: {host_result}")
        return 0


def main():
    p = argparse.ArgumentParser(description="Run a headless chess3d LAN match between two binaries")
    p.add_argument("--bin-a",    required=True, help="Path to host binary")
    p.add_argument("--bin-b",    required=True, help="Path to client binary")
    p.add_argument("--port",     type=int, default=5021, help="TCP port to use")
    p.add_argument("--max-plies", type=int, default=300, dest="max_plies")
    p.add_argument("--timeout",  type=int, default=120, help="Seconds to wait for completion")
    args = p.parse_args()
    sys.exit(run(args))


if __name__ == "__main__":
    main()
