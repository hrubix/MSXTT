/*
 * MSXTT — NOS Teletekst viewer. MSX1 SCREEN 2 or MSX2 SCREEN 5.
 */
#include "tt.h"
#include "bios.h"
#include "dos.h"
#include "gfx.h"
#include "splash.h"

#define TT_OV_COL 36

static const u8 g_DigitKey[10] = {
	KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9
};

static const u8 g_PadKey[10] = {
	KEY_NUM_0, KEY_NUM_1, KEY_NUM_2, KEY_NUM_3, KEY_NUM_4,
	KEY_NUM_5, KEY_NUM_6, KEY_NUM_7, KEY_NUM_8, KEY_NUM_9
};

#define ACT_ESC   0
#define ACT_STOP  1
#define ACT_F1    2
#define ACT_F2    3
#define ACT_F3    4
#define ACT_F4    5
#define ACT_F5    6
#define ACT_LEFT  7
#define ACT_RIGHT 8
#define ACT_UP    9
#define ACT_DOWN  10
#define ACT_HOME  11
#define ACT_BS    12
#define ACT_DEL   13
#define ACT_COUNT 14

#define ACT_ABORT_MASK ((u16)((1 << ACT_ESC) | (1 << ACT_STOP)))

static const u8 g_ActKey[ACT_COUNT] = {
	KEY_ESC, KEY_STOP, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
	KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_HOME, KEY_BS, KEY_DEL
};

static u16 g_Page;
static u8 g_Sub;
static u8 g_HavePage;
static u8 g_NetOk;
static u8 g_SilentFetch;
static u16 g_RefreshStart;
static u8 g_EntryN;
static u8 g_Entry[3];
static u16 g_DigDown;
static u16 g_ActDown;
static u16 g_ActEdge;
static u8 g_PadOk;
static u8 g_VideoOn;

static void OverlayPage(u16 page, u8 fg)
{
	Gfx_DrawOverlay((u8)(page / 100), (u8)((page / 10) % 10), (u8)(page % 10), fg);
}

static void OverlayEntry(u8 fg)
{
	u8 a = 0xFF, b = 0xFF, c = 0xFF;

	if (g_EntryN >= 1)
		a = g_Entry[0];
	if (g_EntryN >= 2)
		b = g_Entry[1];
	if (g_EntryN >= 3)
		c = g_Entry[2];
	Gfx_DrawOverlay(a, b, c, fg);
}

static void PutMsg(u8 row, const c8* s)
{
	u8 col = 2;

	while (*s && (col < TT_COLS))
	{
		u16 i = (u16)((u16)row * TT_COLS + col);
		g_Cells[i * 2] = (u8)*s++;
		g_Cells[i * 2 + 1] = (u8)((7 << 4) | 0);
		col++;
	}
}

static i8 DigitEdge(void)
{
	u16 now = 0;
	u8 i;
	i8 found = -1;

	for (i = 0; i < 10; ++i)
	{
		if (BIOS_IsKeyPressed(g_DigitKey[i]))
			now |= (u16)(1 << i);
		else if (g_PadOk && BIOS_IsKeyPressed(g_PadKey[i]))
			now |= (u16)(1 << i);
	}
	for (i = 0; i < 10; ++i)
	{
		if ((now & (u16)(1 << i)) && !(g_DigDown & (u16)(1 << i)))
		{
			found = (i8)i;
			break;
		}
	}
	g_DigDown = now;
	return found;
}

static void ScanActions(void)
{
	u16 now = 0;
	u8 i;

	for (i = 0; i < ACT_COUNT; ++i)
	{
		if (BIOS_IsKeyPressed(g_ActKey[i]))
			now |= (u16)(1 << i);
	}
	g_ActEdge |= (u16)(now & ~g_ActDown);
	g_ActDown = now;
}

static i8 NextAction(void)
{
	u8 i;

	for (i = 0; i < ACT_COUNT; ++i)
	{
		if (g_ActEdge & (u16)(1 << i))
		{
			g_ActEdge &= (u16)~(1 << i);
			return (i8)i;
		}
	}
	return -1;
}

static bool DigitPending(void)
{
	u8 i;

	for (i = 0; i < 10; ++i)
	{
		if (BIOS_IsKeyPressed(g_DigitKey[i])
			|| (g_PadOk && BIOS_IsKeyPressed(g_PadKey[i])))
		{
			if (!(g_DigDown & (u16)(1 << i)))
				return TRUE;
		}
	}
	return FALSE;
}

bool TT_PollAbort(void)
{
	ScanActions();
	if (g_SilentFetch)
		return (g_ActEdge || DigitPending()) ? TRUE : FALSE;
	DigitEdge();
	if (g_ActEdge & ACT_ABORT_MASK)
	{
		g_ActEdge = 0;
		return TRUE;
	}
	g_ActEdge = 0;
	return FALSE;
}

static void ArmRefresh(void)
{
	g_RefreshStart = TT_Jiffy();
}

static bool RefreshDue(void)
{
	u16 ticks = (u16)(TT_REFRESH_DEFAULT * TT_JIFFY_HZ);

	return TT_TimedOut(g_RefreshStart, ticks);
}

static void ShowErr(u8 err, u16 page)
{
	TT_ClearCells();
	if (err == TT_ERR_TCP)
	{
		PutMsg(12, "TCP ERR");
		PutMsg(13, g_TTHost);
	}
	else if (err == TT_ERR_HTTP)
		PutMsg(12, "HTTP ERR");
	else if (err == TT_ERR_SIZE)
		PutMsg(12, "SIZE ERR");
	else if (err == TT_ERR_PARSE)
		PutMsg(12, "PARSE ERR");
	else if (err == TT_ERR_DNS)
		PutMsg(12, "DNS ERR");
	else
		PutMsg(12, "FETCH ERR");
	Gfx_DrawPage();
	OverlayPage(page, 1);
}

static void ShowCurrent(void)
{
	if (!g_VideoOn)
		return;
	if (g_HavePage)
		OverlayPage(g_Page, 7);
	else
		Gfx_DrawOverlay(0xFF, 0xFF, 0xFF, g_NetOk ? 7 : 1);
}

static void SilentRefresh(void)
{
	u8 err;

	Mem_Copy(g_Cells, g_PrevCells, (u16)(TT_CELLS * 2));
	g_SilentFetch = 1;
	err = TT_FetchPage(g_Page, g_Sub);
	g_SilentFetch = 0;
	ArmRefresh();
	if (err == TT_OK)
	{
		if (TT_CellsChanged())
		{
			Gfx_DrawPage();
			OverlayPage(g_Page, 7);
		}
		return;
	}
	if (err == TT_ERR_PARSE)
		Mem_Copy(g_PrevCells, g_Cells, (u16)(TT_CELLS * 2));
}

static u8 LoadPage(u16 page, u8 sub)
{
	u8 err;

	OverlayPage(page, 3);
	Gfx_WaitCmd();
	err = TT_FetchPage(page, sub);
	ArmRefresh();
	if (err == TT_OK)
	{
		g_Page = page;
		g_Sub = sub;
		g_HavePage = 1;
		Gfx_DrawPage();
		OverlayPage(g_Page, 7);
		return TT_OK;
	}
	if (err == TT_ERR_CANCEL)
	{
		ShowCurrent();
		return err;
	}
	if (g_HavePage)
		OverlayPage(page, 1);
	else
		ShowErr(err, page);
	return err;
}

static void GoLink(const TT_Link* l)
{
	if (!g_NetOk || !l->page || !g_VideoOn)
		return;
	g_EntryN = 0;
	LoadPage(l->page, l->sub);
}

void main(void)
{
	u8 boot_err;

	g_Page = 100;
	g_Sub = 0;
	g_HavePage = 0;
	g_EntryN = 0;
	g_DigDown = 0;
	g_ActDown = 0;
	g_ActEdge = 0;
	g_NetOk = 0;
	g_SilentFetch = 0;
	g_RefreshStart = 0;
	g_VideoOn = 0;
	g_PadOk = (g_NEWKEY[9] && g_NEWKEY[10]) ? 1 : 0;

	EnableInterrupt();
	TT_AfterUnapi();
	Splash_Detect();
	Splash_Show(0);

	TT_LoadCfg();
	boot_err = TT_InitNet();
	g_NetOk = (boot_err == TT_OK) ? 1 : 0;
	if (boot_err == TT_ERR_UNAPI)
	{
		Splash_Show(1);
	}
	else if (g_IsMsx1)
	{
		if (g_NetOk)
			boot_err = TT_FetchPage(100, 0);
		Gfx_InitVideo();
		g_VideoOn = 1;
		if (boot_err == TT_OK)
		{
			g_Page = 100;
			g_Sub = 0;
			g_HavePage = 1;
			ArmRefresh();
			Gfx_DrawPage();
			OverlayPage(g_Page, 7);
		}
		else
			ShowErr(boot_err, 100);
	}
	else
	{
		Splash_Pause();
		Gfx_InitVideo();
		g_VideoOn = 1;
		if (g_NetOk)
			LoadPage(100, 0);
		else
			ShowErr(boot_err, 100);
	}

	while (1)
	{
		i8 a;
		i8 d;

		TT_WaitTick();
		ScanActions();
		while ((a = NextAction()) >= 0)
		{
			if ((a == ACT_ESC) || (a == ACT_STOP))
			{
				if (g_EntryN)
				{
					g_EntryN = 0;
					ShowCurrent();
					continue;
				}
				Gfx_RestoreDOS();
				return;
			}
			if ((a == ACT_BS) || (a == ACT_DEL))
			{
				if (g_EntryN && g_VideoOn)
				{
					g_EntryN--;
					if (g_EntryN)
						OverlayEntry(7);
					else
						ShowCurrent();
				}
				continue;
			}
			if (!g_NetOk || !g_VideoOn)
				continue;
			switch (a)
			{
			case ACT_F1:
			case ACT_F2:
			case ACT_F3:
			case ACT_F4:
				GoLink(&g_Fast[a - ACT_F1]);
				break;
			case ACT_F5:
				if (g_HavePage)
				{
					g_EntryN = 0;
					LoadPage(g_Page, g_Sub);
				}
				break;
			case ACT_LEFT:
				GoLink(&g_NavPrev);
				break;
			case ACT_RIGHT:
				GoLink(&g_NavNext);
				break;
			case ACT_UP:
				GoLink(&g_NavNextSub);
				break;
			case ACT_DOWN:
				GoLink(&g_NavPrevSub);
				break;
			case ACT_HOME:
				g_EntryN = 0;
				LoadPage(100, 0);
				break;
			}
		}

		d = DigitEdge();
		if ((d >= 0) && g_NetOk && g_VideoOn)
		{
			if (g_EntryN >= 3)
				g_EntryN = 0;
			if (g_EntryN == 0)
			{
				g_Entry[1] = 0;
				g_Entry[2] = 0;
			}
			g_Entry[g_EntryN++] = (u8)d;
			OverlayEntry(7);
			if (g_EntryN >= 3)
			{
				u16 page = (u16)((u16)g_Entry[0] * 100 + (u16)g_Entry[1] * 10 + g_Entry[2]);
				g_EntryN = 0;
				LoadPage(page, 0);
			}
		}

		if (g_NetOk && g_HavePage && g_VideoOn && !g_EntryN && RefreshDue())
			SilentRefresh();
	}
}
