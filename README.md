# MSXTT - beta

MSXTT is a live NOS Teletekst viewer for MSX1 and MSX2 computers, bringing the classic Dutch Teletekst experience to your loved MSX computer. MSXTT is a combined NOS Teletekst viewer for MSX, detecting the machine at boot: **MSX1 → SCREEN 2**, anything else (MSX2, MSX2+, turbo R) → **SCREEN 5**.


| Binary        | Use                                                                  |
| ------------- | -------------------------------------------------------------------- |
| `MSXTTPP.COM` | Real MSX + Pico+: HTTPS to `teletekst-data.nos.nl` (cart UNAPI TLS). |
| `MSXTT.ROM`   | t.b.d.                                                               |


Version **0.9.227 beta**. Made by [rubikonlab.com](https://rubikonlab.com).

## Keys

- `0`–`9` (row or keypad): type a 3-digit page; it loads on the third digit
- `BS`, `DEL`: erase last digit
- `ESC`, `STOP`: **COM** — abort fetch, else clear typed digits, else return to DOS; **ROM** — hard reboot
- `F1`–`F4`: fastext red, green, yellow, cyan
- `F5`: reload current page
- `HOME`: page 100
- left / right: previous / next page
- up / down: previous / next subpage



## MSX1 vs MSX2

1. **Screen.** MSX1 uses SCREEN 2 (TMS9918, 256×192). MSX2 uses SCREEN 5 (V9938, 256×212). Both use the same 6×8 cell and 40 columns.
2. **Rows.** Screen 5 shows 40×25. Screen 2 only has 24 rows, so the last (fastext) row is not drawn. `F1`–`F4` still follow the fastext links.
3. **Colours.** NOS / World System Teletext uses 8 inks (0–7). MSX2 loads them as VDP RGB. MSX1 cannot change the TMS9918 palette; maps each NOS ink to a fixed TMS index.


| NOS / teletext | MSX2 RGB | MSX1 TMS index |
| -------------- | -------- | -------------- |
| 0 black        | (0,0,0)  | 1 black        |
| 1 red          | (7,0,0)  | 8 medium red   |
| 2 green        | (0,7,0)  | 2 medium green |
| 3 yellow       | (7,7,0)  | 10 dark yellow |
| 4 blue         | (0,0,7)  | 4 dark blue    |
| 5 magenta      | (7,0,7)  | 13 magenta     |
| 6 cyan         | (0,7,7)  | 7 cyan         |
| 7 white        | (7,7,7)  | 15 white       |


The SCREEN 0 splash uses MSX1 black / green / red (TMS 1 / 2 / 8).

## Font

Official teletext glyphs are **SAA5050** 5×9 (Mullard/Philips). Bitmap source: Teletext50 / Bedstead ([github.com/glxxyz/bedstead](https://github.com/glxxyz/bedstead), CC0). A real SAA5050 cell is 5 ink dots plus a 1-dot gap (6 wide) and 9 rows, often shown ~12×10 after TV rounding.

MSX cells are **6×8** so 40 columns fit 240 px and 25 rows fit Screen 5 (200 px plus margin). The baked font in `src/tt/tt_font.h` takes the 5 ink bits into bits 7–3, leaves bit 2 as a gutter, and **drops SAA5050 row 8**. Descenders (`g p q y`) lose that row, so those four letters come from MSXgl’s 6×8 sample; `j` is a small custom bitmap. Latin-1 accents use Teletext50 G2 packed the same way.

## Build

```
build.bat
```

Writes `build/MSXTT.COM`, `build/MSXTT.DSK`, `emul/dos2/msxtt.com`, and `emul/dsk/MSXTT.DSK` (`AUTOEXEC.BAT`, `UNAPI.COM`, `MSXTT.COM`, `TT.CFG` → local proxy `127.0.0.1:8080`). Refresh is 20 seconds (not configurable). Host proxy: `tools/run-tt-proxy.bat` (Windows Python on `127.0.0.1:8080` so openMSX can connect; use `--wsl` only if you need a WSL listener). Local overrides in `tools/tt-pages/` (page **899** decoder test + subpages); other pages still proxy to NOS. Character audit: `python tools/check-nos-chars.py --out docs/nos-char-audit.txt`.

```
build-msxttpp.bat
```

Writes `build/MSXTTPP.COM` only (Pico+ TLS; copy to your Nextor disk yourself). Do not run `UNAPI.COM` with it — WiFi/UNAPI is on the cart.

```
build-msxtt-rom.bat
```

Writes `build/MSXTT.ROM` and `emul/rom/MSXTT.ROM` (16 KB page-2 cart, direct HTTPS to NOS). Run: `run-msxtt-rom.bat`.