#!/usr/bin/env python3
"""MCWF Teletekst page 333 mockup — layout cloned from NOS page 100.

No logo graphic. Cells only (6x8 font + G1 mosaics for separators/frame).
"""
from __future__ import annotations

import re
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "mockups"
FONT_C = ROOT / "src" / "tt" / "tt_font.c"

W, H = 256, 212
COLS, ROWS = 40, 25
CELL_W, CELL_H = 6, 8
ORIGIN_X, ORIGIN_Y = 8, 6

NOS = {
    0: (0, 0, 0),
    1: (255, 0, 0),
    2: (0, 255, 0),
    3: (255, 255, 0),
    4: (0, 0, 255),
    5: (255, 0, 255),
    6: (0, 255, 255),
    7: (255, 255, 255),
}

# NOS page 100 frame mosaics (top of logo band + bottom bar under band)
NOS100_TOP = (
    "20 20 20 3C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C "
    "2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 2C 34 20"
)
NOS100_BOTTOM = (
    "20 7C 7C 7C 7C 70 73 73 73 73 73 73 73 73 73 73 73 73 73 73 "
    "73 73 73 73 73 73 73 73 73 73 73 73 73 73 73 71 78 7C 7C 7C"
)


def load_tt_font() -> list[list[int]]:
    text = FONT_C.read_text(encoding="utf-8", errors="ignore")
    m = re.search(r"g_TTFont\[128\]\[8\]\s*=\s*\{(.*)\n\};", text, re.S)
    if not m:
        raise SystemExit("g_TTFont not found")
    glyphs: list[list[int]] = []
    for block in re.finditer(r"\{([^}]*)\}", m.group(1)):
        nums = [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+|\d+", block.group(1))]
        if len(nums) == 8:
            glyphs.append(nums)
        if len(glyphs) == 128:
            break
    if len(glyphs) != 128:
        raise SystemExit(f"expected 128 glyphs, got {len(glyphs)}")
    return glyphs


def mosaic_pattern(bits: int) -> list[int]:
    h = (3, 3, 2)
    lb = (0, 2, 4)
    rb = (1, 3, 5)
    dst: list[int] = []
    for row in range(3):
        left = (bits >> lb[row]) & 1
        right = (bits >> rb[row]) & 1
        pat = (0xE0 if left else 0) | (0x1C if right else 0)
        for _ in range(h[row]):
            dst.append(pat)
    return dst


def code_to_bits(code: int) -> int:
    return (code & 0x1F) | ((code & 0x40) >> 1)


def put_pixel(img: Image.Image, x: int, y: int, ink: int) -> None:
    if 0 <= x < W and 0 <= y < H:
        img.putpixel((x, y), NOS[ink & 7])


def draw_pattern(
    img: Image.Image, col: int, row: int, pat: list[int], fg: int, bg: int
) -> None:
    x0 = ORIGIN_X + col * CELL_W
    y0 = ORIGIN_Y + row * CELL_H
    for dy in range(8):
        byte = pat[dy]
        for dx in range(6):
            on = (byte >> (7 - dx)) & 1
            put_pixel(img, x0 + dx, y0 + dy, fg if on else bg)


def draw_char(
    img: Image.Image,
    font: list[list[int]],
    col: int,
    row: int,
    ch: str,
    fg: int,
    bg: int,
) -> None:
    code = ord(ch) if ord(ch) <= 127 else 32
    draw_pattern(img, col, row, font[code], fg, bg)


def draw_mosaic_code(
    img: Image.Image, col: int, row: int, code: int, fg: int, bg: int
) -> None:
    if code == 0x20:
        draw_pattern(img, col, row, [0] * 8, fg, bg)
    else:
        draw_pattern(img, col, row, mosaic_pattern(code_to_bits(code)), fg, bg)


def fill_row_bg(
    img: Image.Image, font: list[list[int]], row: int, fg: int, bg: int
) -> None:
    for c in range(COLS):
        draw_char(img, font, c, row, " ", fg, bg)


def draw_text(
    img: Image.Image,
    font: list[list[int]],
    col: int,
    row: int,
    text: str,
    fg: int,
    bg: int,
) -> None:
    for i, ch in enumerate(text[: COLS - col]):
        draw_char(img, font, col + i, row, ch, fg, bg)


def parse_codes(line: str) -> list[int]:
    return [int(x, 16) for x in line.split()]


def draw_mosaic_row(
    img: Image.Image, row: int, codes: list[int], fg: int, bg: int
) -> None:
    for c, code in enumerate(codes[:COLS]):
        draw_mosaic_code(img, c, row, code, fg, bg)


def mcwf_mosaic_band() -> list[list[int]]:
    """6 rows of mosaic codes for a simple MCWF wordmark on blue (NOS-100 style)."""
    sp = 0x20
    rows = [[sp] * COLS for _ in range(6)]
    # Top bar from NOS 100
    rows[0] = parse_codes(NOS100_TOP)

    # Block letters MCWF — 5 mosaic cols each, gap 1, start col 7
    letters = {
        "M": [
            [0x7F, 0x70, 0x20, 0x70, 0x7F],
            [0x7F, 0x35, 0x20, 0x6A, 0x7F],
            [0x7F, 0x20, 0x7F, 0x20, 0x7F],
            [0x7F, 0x20, 0x20, 0x20, 0x7F],
            [0x7F, 0x20, 0x20, 0x20, 0x7F],
        ],
        "C": [
            [0x30, 0x70, 0x70, 0x70, 0x34],
            [0x7F, 0x20, 0x20, 0x20, 0x20],
            [0x7F, 0x20, 0x20, 0x20, 0x20],
            [0x7F, 0x20, 0x20, 0x20, 0x20],
            [0x23, 0x23, 0x23, 0x23, 0x21],
        ],
        "W": [
            [0x7F, 0x20, 0x20, 0x20, 0x7F],
            [0x7F, 0x20, 0x20, 0x20, 0x7F],
            [0x7F, 0x20, 0x7F, 0x20, 0x7F],
            [0x7F, 0x6A, 0x20, 0x35, 0x7F],
            [0x7F, 0x23, 0x23, 0x23, 0x7F],
        ],
        "F": [
            [0x7F, 0x70, 0x70, 0x70, 0x70],
            [0x7F, 0x20, 0x20, 0x20, 0x20],
            [0x7F, 0x70, 0x70, 0x70, 0x20],
            [0x7F, 0x20, 0x20, 0x20, 0x20],
            [0x7F, 0x20, 0x20, 0x20, 0x20],
        ],
    }
    start = 7
    for i, name in enumerate("MCWF"):
        base = start + i * 6
        for dy, line in enumerate(letters[name]):
            for dx, code in enumerate(line):
                rows[1 + dy][base + dx] = code
    return rows


def build_page(font: list[list[int]]) -> Image.Image:
    img = Image.new("RGB", (W, H), NOS[0])
    for r in range(ROWS):
        fill_row_bg(img, font, r, 7, 0)

    # Row 0 — NOS 100 geometry: green title, yellow page
    fill_row_bg(img, font, 0, 7, 0)
    draw_text(img, font, 22, 0, "MCWF Teletekst", 2, 0)
    draw_text(img, font, 37, 0, "333", 3, 0)

    # Rows 1–6 — blue band + mosaic MCWF (no pictorial/map logo)
    for r in range(1, 7):
        fill_row_bg(img, font, r, 7, 4)
    for r, codes in enumerate(mcwf_mosaic_band()):
        draw_mosaic_row(img, 1 + r, codes, 7, 4)

    # Row 7 — NOS-style white mosaic bar on black
    draw_mosaic_row(img, 7, parse_codes(NOS100_BOTTOM), 7, 0)

    # Rows 8–15 — blue on white headlines + blanks (NOS 100)
    news = [
        (8, "   CLUBDAG 26 SEPTEMBER 2026      334"),
        (9, None),
        (10, "   NOS Teletekst voor MSX!        339"),
        (11, None),
        (12, "   De Huesmolen, Hoorn            335"),
        (13, None),
        (14, "   Toegang gratis / demo / PD-bak 336"),
        (15, None),
    ]
    for row, text in news:
        fill_row_bg(img, font, row, 4, 7)
        if text:
            draw_text(img, font, 0, row, (text + " " * 40)[:40], 4, 7)

    # Row 16 — separator
    draw_mosaic_row(img, 16, [0x73] * COLS, 4, 0)

    # Rows 17–20 — index
    index = [
        (17, "     clubdag    334   teletekst    339"),
        (18, "     route      335   PD-bak       336"),
        (19, "     agenda     337   contact      338"),
        (20, "     facebook   338   website      340"),
    ]
    for row, text in index:
        fill_row_bg(img, font, row, 7, 4)
        t = (text + " " * 40)[:40]
        draw_text(img, font, 0, row, t, 7, 4)
        for col in (15, 34):
            chunk = t[col : col + 3]
            if chunk.strip().isdigit():
                draw_text(img, font, col, row, chunk, 4, 7)

    fill_row_bg(img, font, 21, 7, 0)
    draw_mosaic_row(img, 22, [0x73] * COLS, 4, 0)

    fill_row_bg(img, font, 23, 7, 4)
    draw_text(img, font, 8, 23, "copyright  M C W F  2026", 7, 4)

    # Row 24 fastext — NOS colours
    fill_row_bg(img, font, 24, 7, 0)
    draw_text(img, font, 1, 24, "nieuws", 1, 0)
    draw_text(img, font, 11, 24, "clubdag", 2, 0)
    draw_text(img, font, 21, 24, "route", 3, 0)
    draw_text(img, font, 31, 24, "weer", 6, 0)

    return img


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    font = load_tt_font()
    img = build_page(font)
    native = OUT / "msxwf-333.png"
    big = OUT / "msxwf-333-8x.png"
    img.save(native)
    img.resize((W * 8, H * 8), Image.Resampling.NEAREST).save(big)
    print(f"wrote {native}")
    print(f"wrote {big}")


if __name__ == "__main__":
    main()
