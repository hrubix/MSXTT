/*
 * Video backends: Screen 2 (MSX1) and Screen 5 (MSX2).
 */
#pragma once

#include "msxgl.h"

extern u8 g_IsMsx1;
extern u8 g_Mosaic[64 * 8];

void Gfx_InitMosaics(void);
void Gfx_InitVideo(void);
void Gfx_RestoreDOS(void);
void Gfx_DrawPage(void);
void Gfx_DrawOverlay(u8 d0, u8 d1, u8 d2, u8 fg);
void Gfx_WaitCmd(void);

void Scr5_InitVideo(void);
void Scr5_RestoreDOS(void);
void Scr5_DrawPage(void);
void Scr5_DrawOverlay(u8 d0, u8 d1, u8 d2, u8 fg);

void Scr2_InitVideo(void);
void Scr2_RestoreDOS(void);
void Scr2_DrawPage(void);
void Scr2_DrawOverlay(u8 d0, u8 d1, u8 d2, u8 fg);
