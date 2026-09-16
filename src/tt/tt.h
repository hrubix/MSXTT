/*
 * MSXTT live Teletekst — shared types (MSX1 Screen 2 / MSX2 Screen 5).
 */
#pragma once

#include "msxgl.h"

#if (TARGET == TARGET_ROM_16K_P2)
#define TT_ROM 1
#else
#define TT_ROM 0
#endif

#define TT_COLS      40
#define TT_ROWS      25
#define TT_SCR2_ROWS 24
#define TT_CELL_W    6
#define TT_CELL_H    8
#define TT_ORIGIN_X  8
#define TT_HTTP_MAX  8192
#define TT_HOST_MAX  64
#define TT_CELLS     (TT_COLS * TT_ROWS)

#define TT_OK        0
#define TT_ERR_UNAPI 1
#define TT_ERR_CFG   2
#define TT_ERR_DNS   3
#define TT_ERR_TCP   4
#define TT_ERR_HTTP  5
#define TT_ERR_SIZE  6
#define TT_ERR_PARSE 7
#define TT_ERR_CANCEL 8

#define TT_TIMEOUT_TICKS 500
#define TT_REFRESH_DEFAULT 20
#define TT_REFRESH_MAX     600
#define TT_JIFFY_HZ        50

typedef struct
{
	u16 page;
	u8  sub;
} TT_Link;

/* glyph, attr (fg<<4|bg). 0x80-0xBF mosaic (low 6 bits); 0xC0-0xFF Latin-1. */
#if TT_ROM
extern u8 __at(0x6000) g_Cells[TT_CELLS * 2];
extern u8 __at(0x6800) g_PrevCells[TT_CELLS * 2];
extern c8 __at(0x4000) g_Http[TT_HTTP_MAX];
#else
extern u8 g_Cells[TT_CELLS * 2];
extern u8 __at(0x8200) g_PrevCells[TT_CELLS * 2];
/* Past UNAPI page-2 I/O (8000h) and g_PrevCells (8200h–89CFh). */
extern c8 __at(0x8A00) g_Http[TT_HTTP_MAX];
#endif
extern c8 g_TTHost[TT_HOST_MAX + 1];
extern u16 g_TTPort;
extern u16 g_RefreshSec;

extern TT_Link g_NavNext;
extern TT_Link g_NavPrev;
extern TT_Link g_NavNextSub;
extern TT_Link g_NavPrevSub;
extern TT_Link g_Fast[4];

void TT_AfterUnapi(void);
#if TT_ROM
#define TT_Dbg(code) ((void)(code))
#else
#define TT_Dbg(code) ((void)0)
#endif
void TT_WaitTick(void);
u16  TT_Jiffy(void);
bool TT_TimedOut(u16 start, u16 ticks);

void TT_LoadCfg(void);
u8   TT_InitNet(void);
u8   TT_FetchPage(u16 page, u8 sub);
void TT_PageDigits(u16 page, u8* d); /* d[0]=hundreds, d[1]=tens, d[2]=ones */

bool TT_ParseHttpJson(const c8* json, u16 len);
void TT_ParseNav(const c8* json, u16 len);
void TT_ClearCells(void);
bool TT_CellsChanged(void);

bool TT_PollAbort(void);
