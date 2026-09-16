/*
 * Shared mosaics and Screen 2 / Screen 5 dispatch.
 */
#include "gfx.h"
#include "tt.h"
#include "tt_font.h"
#include "dos.h"
#include "bios.h"

u8 g_IsMsx1;
u8 g_Mosaic[64 * 8];

static void MosaicPattern(u8 bits, u8* dst)
{
	u8 i, row, left, right, pat;
	const u8 h[3] = { 3, 3, 2 };
	const u8 lb[3] = { 0, 2, 4 };
	const u8 rb[3] = { 1, 3, 5 };

	for (row = 0; row < 3; ++row)
	{
		left = (u8)((bits >> lb[row]) & 1);
		right = (u8)((bits >> rb[row]) & 1);
		pat = 0;
		if (left)
			pat |= 0xE0;
		if (right)
			pat |= 0x1C;
		for (i = 0; i < h[row]; ++i)
			*dst++ = pat;
	}
}

void Gfx_InitMosaics(void)
{
	u8 bits;

	for (bits = 0; bits < 64; ++bits)
		MosaicPattern(bits, g_Mosaic + ((u16)bits * 8));
}

const u8* Gfx_CellPattern(u8 glyph)
{
	if (glyph >= 0xC0)
		return g_Latin1[(u8)(glyph - 0xC0)];
	if (glyph >= 0x80)
		return g_Mosaic + ((u16)(glyph & 0x3F) * 8);
	return g_TTFont[glyph];
}

void Gfx_InitVideo(void)
{
	Gfx_InitMosaics();
	if (g_IsMsx1)
		Scr2_InitVideo();
	else
		Scr5_InitVideo();
}

void Gfx_RestoreDOS(void)
{
	/* BIOS COLOR 15,4,4 (white on blue). Splash left FORCLR/BAKCLR as green/black. */
	g_FORCLR = 15;
	g_BAKCLR = 4;
	g_BDRCLR = 4;
	if (g_IsMsx1)
		Scr2_RestoreDOS();
	else
		Scr5_RestoreDOS();
	DOS_InterSlotCall(g_MNROM, R_CHGCLR);
}

void Gfx_DrawPage(void)
{
	if (g_IsMsx1)
		Scr2_DrawPage();
	else
		Scr5_DrawPage();
}

void Gfx_DrawOverlay(u8 d0, u8 d1, u8 d2, u8 fg)
{
	if (g_IsMsx1)
		Scr2_DrawOverlay(d0, d1, d2, fg);
	else
		Scr5_DrawOverlay(d0, d1, d2, fg);
}

void Gfx_WaitCmd(void)
{
	if (!g_IsMsx1)
		VDP_CommandWait();
}
