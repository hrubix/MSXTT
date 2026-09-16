/*
 * SCREEN 0 splash (MSX1-compatible). Black bg, green/red from g_NosToTms.
 */
#include "splash.h"
#include "gfx.h"
#include "tt.h"
#include "bios.h"
#include "dos.h"
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
	DOS_InterSlotCall(g_MNROM, R_CHGCLR);
}

static void PutCh(c8 c)
{
	BIOS_TextPrintChar(c);
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

	while (*p++)
		len++;
	if (len > 40)
		len = 40;
	col = (u8)((40 - len) / 2 + 1);
	Locate(row, col);
	PutStr(s);
}

void Splash_Detect(void)
{
	g_IsMsx1 = (Sys_GetMSXVersion() == 0) ? 1 : 0;
}

void Splash_Show(u8 unapi_miss)
{
	c8 line[40];
	const c8* p;
	const c8* v;
	u8 i;

	DOS_InterSlotCall(g_MNROM, R_INITXT);
	SetInk(SPL_GREEN);

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
		PutCentered(14, "Starting MSX-1 screen 2");
	else
		PutCentered(14, "Starting MSX-2 screen 5");

	if (unapi_miss)
	{
		SetInk(SPL_RED);
		PutCentered(16, "No MSX-UNAPI found");
		PutCentered(17, "msxpico.com");
		SetInk(SPL_GREEN);
	}
}

void Splash_Pause(void)
{
	u8 i;

	for (i = 0; i < 50; i++)
		TT_WaitTick();
}
