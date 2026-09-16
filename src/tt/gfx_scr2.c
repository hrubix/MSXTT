/*
 * MSX1 SCREEN 2 blit (from MSX1TT). 24 visible rows.
 */
#include "gfx.h"
#include "tt.h"
#include "bios.h"
#include "dos.h"
#include "tt_font.h"

#define TT_OV_COL 36

static const u8 g_NosToTms[8] = {
	1,  /* black */
	8,  /* medium red */
	2,  /* medium green */
	10, /* dark yellow */
	4,  /* dark blue */
	13, /* magenta */
	7,  /* cyan */
	15  /* white */
};

static u8 MapAttr(u8 nos_attr)
{
	u8 fg = g_NosToTms[(u8)((nos_attr >> 4) & 7)];
	u8 bg = g_NosToTms[(u8)(nos_attr & 7)];
	return (u8)((fg << 4) | bg);
}

static const u8* CellPattern(u8 glyph)
{
	if (glyph >= 0xC0)
		return g_Latin1[(u8)(glyph - 0xC0)];
	if (glyph >= 0x80)
		return g_Mosaic + ((u16)(glyph & 0x3F) * 8);
	return g_TTFont[glyph];
}

u8 g_MapAttr[256];
const u8* g_GlyphPat[256];
u8 g_TileCell[30][8];
u8 g_TileLeftMask[30];
u8 g_TCell0[30];
u8 g_TCell1[30];
u8 g_THow0[30];
u8 g_THow1[30];
u8 g_Scr2Pat[256];
u8 g_Scr2Col[256];

static u8 Scr2_EncodeHow(u8 tile, u8 ctile, u8 cshift)
{
	if (ctile == tile)
		return (u8)(cshift >> 1);
	return (u8)(3 + ((u8)(8 - cshift) >> 1));
}

static void Scr2_InitLuts(void)
{
	u16 i;
	u8 tx;
	u8 b;
	u8 c;
	u8 cell_tile[40];
	u8 cell_shift[40];

	for (i = 0; i < 256; ++i)
	{
		g_MapAttr[i] = MapAttr((u8)i);
		g_GlyphPat[i] = CellPattern((u8)i);
	}

	for (c = 0; c < TT_COLS; ++c)
	{
		u8 px = (u8)(TT_ORIGIN_X + (u8)(c * TT_CELL_W));
		cell_tile[c] = (u8)(px >> 3);
		cell_shift[c] = (u8)(px & 7);
	}

	for (tx = 1; tx < 31; ++tx)
	{
		u8 px0 = (u8)(tx * 8);
		u8 left;
		u8 mask = 0;
		u8 c0;
		u8 c1;

		for (b = 0; b < 8; ++b)
			g_TileCell[tx - 1][b] =
				(u8)(((u8)(px0 + b) - TT_ORIGIN_X) / TT_CELL_W);
		left = g_TileCell[tx - 1][0];
		for (b = 0; b < 8; ++b)
		{
			if (g_TileCell[tx - 1][b] == left)
				mask |= (u8)(0x80 >> b);
		}
		g_TileLeftMask[tx - 1] = mask;

		c0 = g_TileCell[tx - 1][0];
		c1 = g_TileCell[tx - 1][7];
		g_TCell0[tx - 1] = c0;
		g_TCell1[tx - 1] = c1;
		g_THow0[tx - 1] = Scr2_EncodeHow(tx, cell_tile[c0], cell_shift[c0]);
		g_THow1[tx - 1] = Scr2_EncodeHow(tx, cell_tile[c1], cell_shift[c1]);
	}
}

void ExpandRowScr2(const u8* cells);

static void Scr2_WriteRow(u8 row)
{
	u16 pat_addr = (u16)row * 256;
	u16 col_addr = (u16)(VDP_G2_ADDR_CT + (u16)row * 256);

	DisableInterrupt();
	VDP_WriteVRAM_16K(g_Scr2Pat, pat_addr, 256);
	VDP_WriteVRAM_16K(g_Scr2Col, col_addr, 256);
	EnableInterrupt();
}

static void Scr2_InitLayout(void)
{
	u8 row;
	u8 col;
	u8 names[32];

	for (row = 0; row < TT_SCR2_ROWS; ++row)
	{
		u8 base = (u8)((row & 7) * 32);
		for (col = 0; col < 32; ++col)
			names[col] = (u8)(base + col);
		VDP_WriteVRAM_16K(names, (u16)(VDP_G2_ADDR_NT + (u16)row * 32), 32);
	}
	VDP_FillVRAM_16K(0x00, VDP_G2_ADDR_PT, 6144);
	VDP_FillVRAM_16K(g_MapAttr[0], VDP_G2_ADDR_CT, 6144);
}

void Scr2_DrawPage(void)
{
	u8 row;
	const u8* p = g_Cells;
	const u8* q = g_PrevCells;

	DisableInterrupt();
	for (row = 0; row < TT_SCR2_ROWS; ++row)
	{
		u8 dirty = 0;
		u8 i;

		for (i = 0; i < (u8)(TT_COLS * 2); ++i)
		{
			if (p[i] != q[i])
			{
				dirty = 1;
				break;
			}
		}
		if (dirty)
		{
			u16 pat_addr = (u16)row * 256;
			u16 col_addr = (u16)(VDP_G2_ADDR_CT + (u16)row * 256);

			ExpandRowScr2(p);
			VDP_WriteVRAM_16K(g_Scr2Pat, pat_addr, 256);
			VDP_WriteVRAM_16K(g_Scr2Col, col_addr, 256);
		}
		p += (u16)(TT_COLS * 2);
		q += (u16)(TT_COLS * 2);
	}
	EnableInterrupt();
	Mem_Copy(g_Cells, g_PrevCells, (u16)(TT_CELLS * 2));
}

void Scr2_DrawOverlay(u8 d0, u8 d1, u8 d2, u8 fg)
{
	u8 attr = (u8)((fg << 4) | 0);
	u8 g0 = (d0 == 0xFF) ? '.' : (u8)('0' + d0);
	u8 g1 = (d1 == 0xFF) ? '.' : (u8)('0' + d1);
	u8 g2 = (d2 == 0xFF) ? '.' : (u8)('0' + d2);
	u8 save[6];
	u8 i;
	u16 base = (u16)TT_OV_COL * 2;

	for (i = 0; i < 3; ++i)
	{
		save[i * 2] = g_Cells[base + i * 2];
		save[i * 2 + 1] = g_Cells[base + i * 2 + 1];
	}
	g_Cells[base] = g0;
	g_Cells[base + 1] = attr;
	g_Cells[base + 2] = g1;
	g_Cells[base + 3] = attr;
	g_Cells[base + 4] = g2;
	g_Cells[base + 5] = attr;
	ExpandRowScr2(g_Cells);
	Scr2_WriteRow(0);
	for (i = 0; i < 3; ++i)
	{
		g_Cells[base + i * 2] = save[i * 2];
		g_Cells[base + i * 2 + 1] = save[i * 2 + 1];
	}
}

void Scr2_InitVideo(void)
{
	Scr2_InitLuts();

	VDP_SetMode(VDP_MODE_SCREEN2);
	VDP_SetColor(1);
	VDP_Poke_16K(208, VDP_G2_ADDR_SAT);

	VDP_EnableDisplay(FALSE);
	Scr2_InitLayout();
	VDP_EnableDisplay(TRUE);
}

void Scr2_RestoreDOS(void)
{
	DOS_InterSlotCall(g_MNROM, R_INITXT);
	DOS_InterSlotCall(g_MNROM, R_KILBUF);
	EnableInterrupt();
}
