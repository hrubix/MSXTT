/*
 * MSX2 SCREEN 5 blit (from MSX2TTC).
 */
#include "gfx.h"
#include "tt.h"
#include "bios.h"
#include "color.h"
#include "dos.h"
#include "tt_font.h"

#define TT_OV_COL    36
#define TT_ROW_PX    (TT_COLS * TT_CELL_W)
#define TT_ROW_BYTES ((TT_ROW_PX / 2) * TT_CELL_H)

static const u16 g_TTPalette[8] = {
	RGB16(0, 0, 0),
	RGB16(7, 0, 0),
	RGB16(0, 7, 0),
	RGB16(7, 7, 0),
	RGB16(0, 0, 7),
	RGB16(7, 0, 7),
	RGB16(0, 7, 7),
	RGB16(7, 7, 7),
};

static const u16 g_DOSPalette[16] = {
	RGB16(0, 0, 0), RGB16(0, 0, 0), RGB16(1, 6, 1), RGB16(3, 7, 3),
	RGB16(1, 1, 7), RGB16(2, 3, 7), RGB16(5, 1, 1), RGB16(2, 6, 7),
	RGB16(7, 1, 1), RGB16(7, 3, 3), RGB16(6, 6, 1), RGB16(6, 6, 4),
	RGB16(1, 4, 1), RGB16(6, 2, 5), RGB16(5, 5, 5), RGB16(7, 7, 7),
};

static u8 g_Cell[24];
static u8 g_RowBuf[TT_ROW_BYTES];
/* Same 0x8100 window as net g_Chunk; draw and recv never overlap. */
#define TT_LUT_HI 0x81
static u8 __at(0x8100) g_ExpLut[4];

static u8 OriginY(void)
{
	return (u8)((212 - (TT_ROWS * TT_CELL_H)) / 2);
}

static void ExpandPattern(u8* dest, u8 dest_pitch, const u8* pat, u8 fg, u8 bg) __naked
{
	__asm
	di
	push	ix
	ld	ix, #0
	add	ix, sp
	push	af

	ld	a, 8 (ix)
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	c, a
	or	a, 8 (ix)
	ld	(#_g_ExpLut + 0), a
	ld	a, c
	or	a, 7 (ix)
	ld	(#_g_ExpLut + 1), a
	ld	a, 7 (ix)
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	c, a
	or	a, 8 (ix)
	ld	(#_g_ExpLut + 2), a
	ld	a, c
	or	a, 7 (ix)
	ld	(#_g_ExpLut + 3), a

	ld	c, 4 (ix)
	ld	e, 5 (ix)
	ld	d, 6 (ix)
	ld	a, #8
	ld	-1 (ix), a

exp_row:
	ld	a, (de)
	inc	de
	rlca
	rlca
	ld	b, a
	and	a, #0x03
	push	hl
	ld	h, #TT_LUT_HI
	ld	l, a
	ld	a, (hl)
	pop	hl
	ld	(hl), a
	inc	hl

	ld	a, b
	rlca
	rlca
	ld	b, a
	and	a, #0x03
	push	hl
	ld	h, #TT_LUT_HI
	ld	l, a
	ld	a, (hl)
	pop	hl
	ld	(hl), a
	inc	hl

	ld	a, b
	rlca
	rlca
	and	a, #0x03
	push	hl
	ld	h, #TT_LUT_HI
	ld	l, a
	ld	a, (hl)
	pop	hl
	ld	(hl), a

	dec	hl
	dec	hl
	ld	a, l
	add	a, c
	ld	l, a
	jr	nc, exp_pitch
	inc	h
exp_pitch:
	dec	-1 (ix)
	jr	nz, exp_row

	ld	sp, ix
	pop	ix
	pop	hl
	pop	af
	pop	af
	inc	sp
	ei
	jp	(hl)
	__endasm;
}

static void ExpandRow(u8* dest, const u8* cells) __naked
{
	__asm
	di
	push	ix
	ex	de, hl
	ld	hl, #4
	add	hl, sp
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	b, #40
	ld	c, #0xFF

erow_cell:
	ld	a, 1 (iy)
	cp	a, c
	jr	z, erow_lutok
	ld	c, a
	push	bc
	ld	e, a
	and	a, #0x0F
	ld	d, a
	ld	a, e
	rrca
	rrca
	rrca
	rrca
	and	a, #0x0F
	ld	e, a
	ld	a, d
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	l, a
	or	a, d
	ld	(#_g_ExpLut + 0), a
	ld	a, l
	or	a, e
	ld	(#_g_ExpLut + 1), a
	ld	a, e
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	l, a
	or	a, d
	ld	(#_g_ExpLut + 2), a
	ld	a, l
	or	a, e
	ld	(#_g_ExpLut + 3), a
	pop	bc
erow_lutok:
	ld	a, 0 (iy)
	cp	a, #0xC0
	jr	nc, erow_lat
	cp	a, #0x80
	jr	nc, erow_mos
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #_g_TTFont
	add	hl, de
	ex	de, hl
	jr	erow_got
erow_lat:
	add	a, #0x40
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #_g_Latin1
	add	hl, de
	ex	de, hl
	jr	erow_got
erow_mos:
	and	a, #0x3F
	ld	l, a
	ld	h, #0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, #_g_Mosaic
	add	hl, de
	ex	de, hl
	jr	erow_got
erow_got:
	push	bc
	push	iy
	push	ix
	pop	hl
	ld	b, #8
erow_line:
	ld	a, (de)
	inc	de
	rlca
	rlca
	push	de
	ld	c, a
	and	a, #0x03
	push	hl
	ld	h, #TT_LUT_HI
	ld	l, a
	ld	a, (hl)
	pop	hl
	ld	(hl), a
	inc	hl
	ld	a, c
	rlca
	rlca
	ld	c, a
	and	a, #0x03
	push	hl
	ld	h, #TT_LUT_HI
	ld	l, a
	ld	a, (hl)
	pop	hl
	ld	(hl), a
	inc	hl
	ld	a, c
	rlca
	rlca
	and	a, #0x03
	push	hl
	ld	h, #TT_LUT_HI
	ld	l, a
	ld	a, (hl)
	pop	hl
	ld	(hl), a
	dec	hl
	dec	hl
	ld	a, l
	add	a, #0x78
	ld	l, a
	jr	nc, erow_pitch
	inc	h
erow_pitch:
	pop	de
	djnz	erow_line
	pop	iy
	pop	bc
	inc	ix
	inc	ix
	inc	ix
	inc	iy
	inc	iy
	dec	b
	jp	nz, erow_cell
	pop	ix
	ei
	ret
	__endasm;
}

static const u8* CellPattern(u8 glyph, u8* mosaic)
{
	if (glyph >= 0xC0)
		return g_Latin1[(u8)(glyph - 0xC0)];
	if (glyph >= 0x80)
	{
		u8 i, row, left, right, pat, bits;
		const u8 h[3] = { 3, 3, 2 };
		const u8 lb[3] = { 0, 2, 4 };
		const u8 rb[3] = { 1, 3, 5 };
		u8* dst = mosaic;

		bits = (u8)(glyph & 0x3F);
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
		return mosaic;
	}
	return g_TTFont[glyph];
}

static void DrawPattern(u16 x, u8 y, const u8* pat, u8 fg, u8 bg)
{
	ExpandPattern(g_Cell, 3, pat, fg, bg);
	VDP_CommandHMMC(g_Cell, x, y, TT_CELL_W, TT_CELL_H);
}

static void DrawOne(u8 col, u8 row, u8 glyph, u8 attr)
{
	u8 mosaic[8];
	u8 fg = (u8)(attr >> 4);
	u8 bg = (u8)(attr & 0x0F);
	u16 x = (u16)(TT_ORIGIN_X + col * TT_CELL_W);
	u8 y = (u8)(OriginY() + row * TT_CELL_H);

	DrawPattern(x, y, CellPattern(glyph, mosaic), fg, bg);
}

void Scr5_DrawPage(void)
{
	u8 row;
	const u8* p = g_Cells;
	u8 y0 = OriginY();

	for (row = 0; row < TT_ROWS; ++row)
	{
		ExpandRow(g_RowBuf, p);
		p += (u16)(TT_COLS * 2);
		VDP_CommandHMMC(g_RowBuf, TT_ORIGIN_X, (u16)(y0 + row * TT_CELL_H),
			TT_ROW_PX, TT_CELL_H);
	}
	VDP_CommandWait();
}

void Scr5_DrawOverlay(u8 d0, u8 d1, u8 d2, u8 fg)
{
	u8 attr = (u8)((fg << 4) | 0);
	u8 g0 = (d0 == 0xFF) ? '.' : (u8)('0' + d0);
	u8 g1 = (d1 == 0xFF) ? '.' : (u8)('0' + d1);
	u8 g2 = (d2 == 0xFF) ? '.' : (u8)('0' + d2);

	DrawOne(TT_OV_COL, 0, g0, attr);
	DrawOne((u8)(TT_OV_COL + 1), 0, g1, attr);
	DrawOne((u8)(TT_OV_COL + 2), 0, g2, attr);
}

void Scr5_InitVideo(void)
{
	u8 i;

	VDP_SetMode(VDP_MODE_SCREEN5);
	VDP_EnableTransparency(FALSE);
	VDP_DisableSprite();
	VDP_SetColor(0);

	for (i = 0; i < 8; ++i)
		VDP_SetPaletteEntry(i, g_TTPalette[i]);
	for (i = 8; i < 16; ++i)
		VDP_SetPaletteEntry(i, RGB16(0, 0, 0));

	VDP_EnableDisplay(FALSE);
	VDP_CommandHMMV(0, 0, 256, 212, 0x00);
	VDP_CommandWait();
	VDP_EnableDisplay(TRUE);
}

void Scr5_RestoreDOS(void)
{
	u8 i;

	for (i = 0; i < 16; ++i)
		VDP_SetPaletteEntry(i, g_DOSPalette[i]);
	DOS_InterSlotCall(g_MNROM, R_INITXT);
	DOS_InterSlotCall(g_MNROM, R_KILBUF);
	EnableInterrupt();
}
