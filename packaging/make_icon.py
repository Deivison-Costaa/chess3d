#!/usr/bin/env python3
"""Gera packaging/chess3d.png — ícone 256x256 (tabuleiro de xadrez estilizado).

Escreve um PNG RGBA válido sem depender de PIL (só stdlib: zlib + struct).
Reexecutável: `python3 packaging/make_icon.py`.
"""
import struct
import zlib
from pathlib import Path

SIZE = 256
BORDER = 16
CELL = (SIZE - 2 * BORDER) // 8

# Paleta (RGBA)
BG     = (24, 26, 32, 255)      # fundo / moldura escura
LIGHT  = (235, 222, 197, 255)   # casas claras
DARK   = (118, 95, 71, 255)     # casas escuras
ACCENT = (90, 170, 120, 255)    # destaque sutil em algumas casas


def pixel(x: int, y: int):
    if x < BORDER or y < BORDER or x >= SIZE - BORDER or y >= SIZE - BORDER:
        return BG
    cx = (x - BORDER) // CELL
    cy = (y - BORDER) // CELL
    if (cx + cy) % 2 == 0:
        return LIGHT
    # algumas casas escuras recebem o tom de destaque (diagonal central)
    if cx == cy or cx + cy == 7:
        return ACCENT
    return DARK


def main() -> None:
    raw = bytearray()
    for y in range(SIZE):
        raw.append(0)  # filtro "None" por scanline
        for x in range(SIZE):
            raw.extend(pixel(x, y))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)  # 8-bit RGBA
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))

    out = Path(__file__).resolve().parent / "chess3d.png"
    out.write_bytes(png)
    print(f"escrito {out} ({len(png)} bytes)")


if __name__ == "__main__":
    main()
