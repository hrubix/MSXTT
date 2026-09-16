/*
 * MSX2TT — parse NOS Teletekst JSON "content" into 40×25 cells.
 * Logic mirrors tools/tt-bake.py (HTML spans, mosaics, Latin-1 glyphs).
 */
#include "tt.h"

#if TT_ROM
u8 __at(0x6000) g_Cells[TT_CELLS * 2];
u8 __at(0x6800) g_PrevCells[TT_CELLS * 2];
#else
u8 g_Cells[TT_CELLS * 2];
/* Absolute so the extra 2 KB does not grow _DATA into the UNAPI page-2 buffers. */
u8 __at(0x8200) g_PrevCells[TT_CELLS * 2];
#endif

TT_Link g_NavNext;
TT_Link g_NavPrev;
TT_Link g_NavNextSub;
TT_Link g_NavPrevSub;
TT_Link g_Fast[4];

#define STACK_MAX 16
#define TAG_MAX   96
#define ENT_MAX   16

static u8 g_Fg;
static u8 g_Bg;
static u8 g_StackFg[STACK_MAX];
static u8 g_StackBg[STACK_MAX];
static u8 g_Sp;
static u8 g_Row;
static u8 g_Col;
static u8 g_Mode; /* 0 text, 1 tag, 2 entity */
static u8 g_TagN;
static u8 g_EntN;
static c8 g_Tag[TAG_MAX];
static c8 g_Ent[ENT_MAX];
static u8 g_Utf1;

static void Newline(void)
{
	u16 i;

	if (g_Row >= TT_ROWS)
		return;
	while (g_Col < TT_COLS)
	{
		i = (u16)((u16)g_Row * TT_COLS + g_Col);
		g_Cells[i * 2] = 0x20;
		g_Cells[i * 2 + 1] = (u8)((7 << 4) | 0);
		g_Col++;
	}
	g_Row++;
	g_Col = 0;
}

static u8 MosaicGlyph(u8 code)
{
	u8 bits = (u8)((code & 0x1F) | ((code & 0x40) >> 1));
	return (u8)(0x80 | (bits & 0x3F));
}

static u8 MapAscii(u8 o)
{
	if (o == 0xA0)
		return 0x20;
	if ((o >= 0x20) && (o <= 0x7E))
		return o;
	return 0x3F;
}

/* UTF-8 C3 xx is U+00C0–U+00FF. Glyph bytes match those codepoints. */
static u8 MapC3(u8 b)
{
	return (u8)(b + 0x40);
}

/* Case-sensitive; NameEq folds case and would merge Agrave/agrave. */
static bool NameIs(const c8* a, const c8* b)
{
	while (*a && *b)
	{
		if (*a++ != *b++)
			return FALSE;
	}
	return (*a == 0) && (*b == 0);
}

/* HTML Latin-1 names used by NOS (mostly UTF-8 / &#x..; otherwise). */
static const c8 g_LatEnt[] =
	"egrave\0eacute\0ecirc\0euml\0"
	"agrave\0aacute\0auml\0"
	"igrave\0iacute\0iuml\0"
	"ograve\0oacute\0ouml\0"
	"ugrave\0uacute\0uuml\0"
	"ccedil\0ntilde\0"
	"Egrave\0Eacute\0Euml\0"
	"Auml\0Iuml\0Ouml\0Uuml\0Ccedil\0";

static const u8 g_LatCode[] = {
	0xE8, 0xE9, 0xEA, 0xEB,
	0xE0, 0xE1, 0xE4,
	0xEC, 0xED, 0xEF,
	0xF2, 0xF3, 0xF6,
	0xF9, 0xFA, 0xFC,
	0xE7, 0xF1,
	0xC8, 0xC9, 0xCB,
	0xC4, 0xCF, 0xD6, 0xDC, 0xC7
};

static u8 LookupLatin1Ent(void)
{
	const c8* p = g_LatEnt;
	u8 i = 0;

	while (*p)
	{
		if (NameIs(g_Ent, p))
			return g_LatCode[i];
		while (*p)
			p++;
		p++;
		i++;
	}
	return 0;
}

static void EmitGlyph(u8 glyph)
{
	u16 i;

	if (g_Row >= TT_ROWS)
		return;
	if (g_Col >= TT_COLS)
		return;
	i = (u16)((u16)g_Row * TT_COLS + g_Col);
	g_Cells[i * 2] = glyph;
	g_Cells[i * 2 + 1] = (u8)((g_Fg << 4) | g_Bg);
	g_Col++;
}

static void EmitChar(u8 ch)
{
	if (ch == '\r')
		return;
	if (ch == '\n')
	{
		Newline();
		return;
	}
	if (ch == '\t')
		ch = ' ';
	EmitGlyph(MapAscii(ch));
}

static void EmitCodepoint(u16 v)
{
	if ((v >= 0xF000) && (v <= 0xF07F))
		EmitGlyph(MosaicGlyph((u8)(v & 0xFF)));
	else if (v == 0xA0)
		EmitChar(' ');
	else if (v < 128)
		EmitChar((u8)v);
	else if ((v >= 0xC0) && (v <= 0xFF))
		EmitGlyph((u8)v);
	else
		EmitChar('?');
}

static u8 HexVal(c8 c)
{
	if ((c >= '0') && (c <= '9'))
		return (u8)(c - '0');
	if ((c >= 'a') && (c <= 'f'))
		return (u8)(c - 'a' + 10);
	if ((c >= 'A') && (c <= 'F'))
		return (u8)(c - 'A' + 10);
	return 0xFF;
}

static bool NameEq(const c8* a, const c8* b)
{
	while (*b)
	{
		c8 ca = *a++;
		c8 cb = *b++;
		if ((ca >= 'A') && (ca <= 'Z'))
			ca = (c8)(ca + 32);
		if ((cb >= 'A') && (cb <= 'Z'))
			cb = (c8)(cb + 32);
		if (ca != cb)
			return FALSE;
	}
	return TRUE;
}

/* NOS class tokens are lowercase: black..white and bg-*. */
static const c8 g_ColName[] =
	"black\0red\0green\0yellow\0blue\0magenta\0cyan\0white\0";

static void ParseClassToken(const c8* t, u8 n)
{
	u8 bg = 0;
	u8 i;
	const c8* p;

	if ((n >= 3) && (t[0] == 'b') && (t[1] == 'g') && (t[2] == '-'))
	{
		bg = 1;
		t += 3;
		n = (u8)(n - 3);
	}
	p = g_ColName;
	i = 0;
	while (*p)
	{
		u8 k = 0;
		while (p[k] && (k < n) && (p[k] == t[k]))
			k++;
		if (!p[k] && (k == n))
		{
			if (bg)
				g_Bg = i;
			else
				g_Fg = i;
			return;
		}
		while (*p)
			p++;
		p++;
		i++;
	}
}

static void ApplyClasses(const c8* s)
{
	u8 n;
	const c8* t;

	/* s points just past the opening quote; the closing quote ends the list. */
	while (*s && (*s != '"'))
	{
		while ((*s == ' ') || (*s == '\t'))
			s++;
		if (!*s || (*s == '"'))
			break;
		t = s;
		n = 0;
		while (*s && (*s != ' ') && (*s != '\t') && (*s != '"'))
		{
			s++;
			n++;
		}
		if (!n)
			break;
		ParseClassToken(t, n);
	}
}

static void FindClass(const c8* tag)
{
	const c8* p = tag;

	while (*p)
	{
		if (NameEq(p, "class"))
		{
			p += 5;
			while ((*p == ' ') || (*p == '\t'))
				p++;
			if (*p == '=')
			{
				p++;
				while ((*p == ' ') || (*p == '\t'))
					p++;
				if (*p == '"')
				{
					p++;
					ApplyClasses(p);
				}
			}
			return;
		}
		p++;
	}
}

static void TagName(const c8* tag, c8* name, u8 max)
{
	u8 n = 0;
	const c8* p = tag;

	while ((*p == ' ') || (*p == '\t'))
		p++;
	if (*p == '/')
		p++;
	while ((*p == ' ') || (*p == '\t'))
		p++;
	while (*p && (*p != ' ') && (*p != '\t') && (*p != '/') && (n + 1 < max))
	{
		c8 c = *p++;
		if ((c >= 'A') && (c <= 'Z'))
			c = (c8)(c + 32);
		name[n++] = c;
	}
	name[n] = 0;
}

static void ProcessTag(void)
{
	c8 name[8];
	u8 closing;
	const c8* p;

	g_Tag[g_TagN] = 0;
	p = g_Tag;
	while ((*p == ' ') || (*p == '\t'))
		p++;
	closing = (*p == '/') ? 1 : 0;
	TagName(p, name, 8);
	if (closing)
	{
		if (g_Sp)
		{
			g_Sp--;
			g_Fg = g_StackFg[g_Sp];
			g_Bg = g_StackBg[g_Sp];
		}
		return;
	}
	if (NameEq(name, "br") || NameEq(name, "hr"))
	{
		Newline();
		return;
	}
	if (g_Sp < STACK_MAX)
	{
		g_StackFg[g_Sp] = g_Fg;
		g_StackBg[g_Sp] = g_Bg;
		g_Sp++;
	}
	FindClass(p);
}

static void ProcessEntity(void)
{
	u16 v;
	u8 i;

	g_Ent[g_EntN] = 0;
	if (!g_EntN)
	{
		EmitChar('&');
		return;
	}
	if ((g_Ent[0] == '#') && (g_Ent[1] == 'x' || g_Ent[1] == 'X'))
	{
		v = 0;
		i = 2;
		while (g_Ent[i])
		{
			u8 h = HexVal(g_Ent[i]);
			if (h == 0xFF)
				break;
			v = (u16)((v << 4) | h);
			i++;
		}
		EmitCodepoint(v);
		return;
	}
	if (g_Ent[0] == '#')
	{
		v = 0;
		i = 1;
		while ((g_Ent[i] >= '0') && (g_Ent[i] <= '9'))
		{
			v = (u16)(v * 10 + (u8)(g_Ent[i] - '0'));
			i++;
		}
		EmitCodepoint(v);
		return;
	}
	if (NameEq(g_Ent, "nbsp"))
		EmitChar(' ');
	else if (NameEq(g_Ent, "amp"))
		EmitChar('&');
	else if (NameEq(g_Ent, "lt"))
		EmitChar('<');
	else if (NameEq(g_Ent, "gt"))
		EmitChar('>');
	else if (NameEq(g_Ent, "quot"))
		EmitChar('"');
	else
	{
		u8 lat = LookupLatin1Ent();
		if (lat)
			EmitGlyph(lat);
		else
			EmitChar('?');
	}
}

static void Feed(u8 c)
{
	if (g_Utf1)
	{
		u8 lead = g_Utf1;
		g_Utf1 = 0;
		if (lead == 0xC2)
			EmitChar(c == 0xA0 ? ' ' : MapAscii(c));
		else if (lead == 0xC3)
			EmitGlyph(MapC3(c));
		else
			EmitChar('?');
		return;
	}
	if (g_Mode == 1)
	{
		if (c == '>')
		{
			ProcessTag();
			g_Mode = 0;
			g_TagN = 0;
		}
		else if (g_TagN + 1 < TAG_MAX)
			g_Tag[g_TagN++] = (c8)c;
		return;
	}
	if (g_Mode == 2)
	{
		if (c == ';')
		{
			ProcessEntity();
			g_Mode = 0;
			g_EntN = 0;
			return;
		}
		/* Bare "&" in page text (718 "Z O N  &  M A A N") is not an entity. */
		if (!g_EntN && (c != '#')
			&& !((c >= 'A') && (c <= 'Z'))
			&& !((c >= 'a') && (c <= 'z')))
		{
			EmitChar('&');
			g_Mode = 0;
		}
		else
		{
			if (g_EntN + 1 < ENT_MAX)
				g_Ent[g_EntN++] = (c8)c;
			return;
		}
	}
	if (c == '<')
	{
		g_Mode = 1;
		g_TagN = 0;
		return;
	}
	if (c == '&')
	{
		g_Mode = 2;
		g_EntN = 0;
		return;
	}
	if (c >= 0xC2)
	{
		g_Utf1 = c;
		return;
	}
	EmitChar(c);
}

/* Returns the position just past the closing quote of the key "name", or 0. */
static const c8* FindKey(const c8* s, const c8* end, const c8* name)
{
	while (s < end)
	{
		if (*s == '"')
		{
			const c8* p = s + 1;
			const c8* n = name;

			while ((p < end) && *n && (*p == *n))
			{
				p++;
				n++;
			}
			if (!*n && (p < end) && (*p == '"'))
				return p + 1;
		}
		s++;
	}
	return 0;
}

/* Returns the first character of the string value of "name", or 0. */
static const c8* FindField(const c8* s, const c8* end, const c8* name)
{
	const c8* p = FindKey(s, end, name);

	if (!p)
		return 0;
	while ((p < end) && ((*p == ' ') || (*p == '\t')))
		p++;
	if ((p >= end) || (*p != ':'))
		return 0;
	p++;
	while ((p < end) && ((*p == ' ') || (*p == '\t')))
		p++;
	if ((p < end) && (*p == '"'))
		return p + 1;
	return 0;
}

static const c8* FindContent(const c8* s, const c8* end)
{
	return FindField(s, end, "content");
}

/* "101" or "100-3"; an empty value leaves the link cleared. */
static void ParseLinkValue(const c8* s, const c8* end, TT_Link* out)
{
	u16 page = 0;
	u8 sub = 0;
	u8 n = 0;

	out->page = 0;
	out->sub = 0;
	if (!s)
		return;
	while ((s < end) && (*s >= '0') && (*s <= '9') && (n < 3))
	{
		page = (u16)(page * 10 + (u8)(*s++ - '0'));
		n++;
	}
	if (!n)
		return;
	if ((s < end) && (*s == '-'))
	{
		s++;
		n = 0;
		while ((s < end) && (*s >= '0') && (*s <= '9') && (n < 2))
		{
			sub = (u8)(sub * 10 + (u8)(*s++ - '0'));
			n++;
		}
	}
	out->page = page;
	out->sub = sub;
}

static void ParseLinkField(const c8* json, const c8* end, const c8* name, TT_Link* out)
{
	ParseLinkValue(FindField(json, end, name), end, out);
}

void TT_ParseNav(const c8* json, u16 len)
{
	const c8* end = json + len;
	const c8* s;
	u8 i;

	for (i = 0; i < 4; ++i)
	{
		g_Fast[i].page = 0;
		g_Fast[i].sub = 0;
	}
	ParseLinkField(json, end, "prevPage", &g_NavPrev);
	ParseLinkField(json, end, "nextPage", &g_NavNext);
	ParseLinkField(json, end, "prevSubPage", &g_NavPrevSub);
	ParseLinkField(json, end, "nextSubPage", &g_NavNextSub);

	/* fastTextLinks is [{"title":..,"page":".."},..]; the "page" of each entry
	 * in order is the red/green/yellow/cyan order of the bottom row. */
	s = FindKey(json, end, "fastTextLinks");
	if (!s)
		return;
	{
		const c8* alim = s;

		/* The entries hold string values only, so the first ] closes the array
		 * and keeps the scan out of the content body. */
		while ((alim < end) && (*alim != ']'))
			alim++;
		for (i = 0; i < 4; ++i)
		{
			const c8* v = FindField(s, alim, "page");
			if (!v)
				break;
			ParseLinkValue(v, alim, &g_Fast[i]);
			s = v;
		}
	}
}

void TT_ClearCells(void)
{
	u16 i;

	for (i = 0; i < TT_CELLS; i++)
	{
		g_Cells[i * 2] = 0x20;
		g_Cells[i * 2 + 1] = (u8)((7 << 4) | 0);
	}
}

bool TT_CellsChanged(void)
{
	u16 i;

	for (i = 0; i < (u16)(TT_CELLS * 2); i++)
	{
		if (g_Cells[i] != g_PrevCells[i])
			return TRUE;
	}
	return FALSE;
}

bool TT_ParseHttpJson(const c8* json, u16 len)
{
	const c8* s;
	const c8* end;
	const c8* p;

	end = json + len;
	s = FindContent(json, end);
	if (!s)
		return FALSE;

	TT_ClearCells();
	g_Fg = 7;
	g_Bg = 0;
	g_Sp = 0;
	g_Row = 0;
	g_Col = 0;
	g_Mode = 0;
	g_TagN = 0;
	g_EntN = 0;
	g_Utf1 = 0;

	p = s;
	while (p < end)
	{
		u8 c = (u8)*p++;
		if (c == '\\')
		{
			if (p >= end)
				break;
			c = (u8)*p++;
			if (c == 'n')
				Feed('\n');
			else if (c == 'r')
				;
			else if (c == 't')
				Feed(' ');
			else if (c == 'u')
			{
				u8 k;
				u16 v = 0;
				for (k = 0; k < 4; k++)
				{
					u8 h;
					if (p >= end)
						break;
					h = HexVal(*p++);
					if (h == 0xFF)
						break;
					v = (u16)((v << 4) | h);
				}
				EmitCodepoint(v);
			}
			else
				Feed(c);
			continue;
		}
		if (c == '"')
			break;
		Feed(c);
	}

	if (g_Mode == 1)
		ProcessTag();
	if (g_Mode == 2)
		ProcessEntity();

	if (g_Row < TT_ROWS)
		Newline();
	while (g_Row < TT_ROWS)
		Newline();
	return TRUE;
}
