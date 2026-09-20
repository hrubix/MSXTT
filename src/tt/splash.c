/*
 * SCREEN 0 splash (MSX1-compatible). Black bg, green/red from g_NosToTms.
 */
#include "splash.h"
#include "gfx.h"
#include "tt.h"
#include "bios.h"
#include "system.h"
#include "../version.h"

/* TMS9918 indices from msx1tt g_NosToTms: black=1, medium green=2, medium red=8. */
#define SPL_BLACK 1
#define SPL_GREEN 2
#define SPL_RED   8

static void SetInk(u8 fg)
{
	g_FORCLR = fg;
	g_BAKCLR = SPL_BLACK;
	g_BDRCLR = SPL_BLACK;
	BIOS_InterSlotCall(g_MNROM, R_CHGCLR);
}

static u8 s_PutC;

/* Page 0 is DOS RAM, so a direct CHPUT at 00A2h hangs. CALSLT the Main-ROM. */
static void PutCh(c8 c)
{
	s_PutC = (u8)c;
	__asm
		push	ix
		ld		a, (_g_MNROM)
		ld		h, a
		ld		l, #0
		push	hl
		pop		iy
		ld		ix, #R_CHPUT
		ld		a, (_s_PutC)
		call	#R_CALSLT
		ei
		pop		ix
	__endasm;
}

static void PutStr(const c8* s)
{
	while (*s)
		PutCh(*s++);
}

static void Locate(u8 row, u8 col)
{
	g_CSRY = row;
	g_CSRX = col;
}

static void PutCentered(u8 row, const c8* s)
{
	u8 len = 0;
	const c8* p = s;
	u8 col;
	u8 w = g_LINLEN;

	while (*p++)
		len++;
	/* INITXT can leave LINLEN at the machine default (often 37). */
	if ((w < 32) || (w > 80))
		w = 40;
	if (len > w)
		len = w;
	/* 1-based CSRX; spare column on odd widths goes to the left. */
	col = (u8)((w - len + 1) / 2 + 1);
	Locate(row, col);
	PutStr(s);
}

void Splash_Detect(void)
{
	g_IsMsx1 = (Sys_GetMSXVersion() == 0) ? 1 : 0;
}

#if defined(TT_ROM_DBG)
static c8 HexNibble(u8 v)
{
	v &= 0x0F;
	return (c8)((v < 10) ? ('0' + v) : ('A' + v - 10));
}
#endif

void Splash_Show(u8 unapi_miss)
{
	c8 line[40];
	const c8* p;
	const c8* v;
	u8 i;

	g_LINL40 = 40;
	BIOS_InterSlotCall(g_MNROM, R_INITXT);
	/* SCREEN 0 has one foreground for the whole screen (R#7). On UNAPI miss
	 * use medium red for every line; otherwise green. */
	SetInk(unapi_miss ? SPL_RED : SPL_GREEN);
	BIOS_InterSlotCall(g_MNROM, R_ENASCR);

	PutCentered(10, "NOS Teletekst");

	p = "version: ";
	v = APP_VERSION;
	i = 0;
	while (*p)
		line[i++] = *p++;
	while (*v)
		line[i++] = *v++;
	p = " beta";
	while (*p)
		line[i++] = *p++;
	line[i] = 0;
	PutCentered(11, line);
	PutCentered(12, "made by rubikonlab.com");

	if (g_IsMsx1)
		PutCentered(14, "mode: MSX-1 screen 2");
	else
		PutCentered(14, "mode: MSX-2 screen 5");

	if (unapi_miss)
	{
		PutCentered(16, "No MSX-UNAPI found");
		PutCentered(17, "msxpico.com");
#if defined(TT_ROM_DBG)
		/* r=count u=ok h=helper k=HOKVLD; EXTBIO 5 bytes; ARG */
		{
			extern u8 unapi_raw_count;
			extern u16 implementation_count;
			extern u16 ram_helper_call_address;
			c8 d[40];
			u8 j = 0;
			u8 n;
			volatile u8* arg = (volatile u8*)0xF847;
			volatile u8* ext = (volatile u8*)0xFFCA;
			u8 hok = *(volatile u8*)0xFB20;
			d[j++] = 'r';
			d[j++] = HexNibble((u8)(unapi_raw_count >> 4));
			d[j++] = HexNibble(unapi_raw_count);
			d[j++] = ' ';
			d[j++] = 'u';
			d[j++] = HexNibble((u8)((implementation_count >> 4) & 0x0F));
			d[j++] = HexNibble((u8)(implementation_count & 0x0F));
			d[j++] = ' ';
			d[j++] = 'h';
			d[j++] = ram_helper_call_address ? '1' : '0';
			d[j++] = ' ';
			d[j++] = 'k';
			d[j++] = (hok & 1) ? '1' : '0';
			d[j++] = ' ';
			for (n = 0; n < 5; n++)
			{
				d[j++] = HexNibble((u8)(ext[n] >> 4));
				d[j++] = HexNibble(ext[n]);
			}
			d[j] = 0;
			PutCentered(18, d);
			j = 0;
			for (n = 0; n < 6; n++)
			{
				u8 c = arg[n];
				d[j++] = (c >= 32 && c < 127) ? (c8)c : '.';
			}
			d[j] = 0;
			PutCentered(19, d);
		}
#endif
	}
}

#if defined(TT_ROM_DBG)
void Splash_Dbg(u8 code)
{
	c8 line[8];

	line[0] = 'D';
	line[1] = HexNibble((u8)(code >> 4));
	line[2] = HexNibble(code);
	line[3] = 0;
	Locate(18, 1);
	PutStr(line);
}
#endif

void Splash_Pause(void)
{
	u8 i;

	for (i = 0; i < 50; i++)
		TT_WaitTick();
}
