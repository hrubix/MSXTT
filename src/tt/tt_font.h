/* Hybrid 6x8 teletext font. Data lives in tt_font.c (one ROM/COM copy).
 * SAA5050/Teletext50 (CC0) packed 5x9->6x8; g/p/q/y from MSXgl sample6;
 * j is tittle + stem + left bar. Latin-1 from Teletext50 G2.
 */
#pragma once
#include "msxgl.h"

extern const u8 g_TTFont[128][8];
extern const u8 g_Latin1[64][8];
