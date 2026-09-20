/*
 * MSXTT — UNAPI HTTP GET /json/NNN[-S].
 * Host from TT.CFG proxy-url. Refresh is compiled as 20 s.
 */
#include "tt.h"
#if !TT_ROM
#include "dos.h"
#endif
#include "network/unapi_tcp.h"
#if defined(TT_ROM_DBG)
#include "splash.h"
#endif

#ifndef TT_DIRECT_NOS
#define TT_DIRECT_NOS 0
#endif

#define TT_NOS_HOST     "teletekst-data.nos.nl"
#define TT_NOS_HOST_LEN 22
#define TT_NOS_PORT     443
#define TT_TCP_FLAG_TLS 0x04

c8 g_TTHost[TT_HOST_MAX + 1];
u16 g_TTPort;
u16 g_RefreshSec;
static char g_Ip[4];

/*
 * ROM (Pico WIFI+Telnet): page 1 = ESP8266 UNAPI BIOS + mem UART (7F06h/7F07h),
 * page 2 = this cart. Do not place RW buffers in 4000h–BFFFh and do not ENASLT
 * RAM into page 1 after UNAPI (that unmaps the UART and resets within ~1 s).
 * Page-3 layout (stack at F200h): cells C000, prev C7D0, request CFA0,
 * chunk D060, linker DATA from D180, HTTP E020 (4 KB).
 */
#if TT_ROM
__at(0xCFA0) static c8 g_Request[192];
__at(0xD060) static c8 g_Chunk[256];
c8 __at(0xE020) g_Http[TT_HTTP_MAX];
#else
/* DOS2 COM: pin in page 2, past code/data and below the TPA stack. */
__at(0x8000) static c8 g_Request[192];
__at(0x8100) static c8 g_Chunk[256];
c8 __at(0x8A00) g_Http[TT_HTTP_MAX];
#endif
static u8 g_Cancel;

/* A fetch waits up to ten seconds per stage, so every wait loop gives the
 * abort keys a look; the flag keeps the unwind path out of the loops. */
static bool Cancelled(void)
{
	if (!g_Cancel && TT_PollAbort())
		g_Cancel = 1;
	return g_Cancel ? TRUE : FALSE;
}

void TT_AfterUnapi(void)
{
	/* ROM: leave page 1 on the Pico slot so ESP mem-UART stays visible. */
	EnableInterrupt();
}

#if TT_ROM && defined(TT_ROM_DBG)
void TT_DbgImpl(u8 code)
{
	Splash_Dbg(code);
}
#else
void TT_DbgImpl(u8 code)
{
	(void)code;
}
#endif

u16 TT_Jiffy(void)
{
	return ((volatile u8*)0xFC9E)[0] | ((u16)((volatile u8*)0xFC9E)[1] << 8);
}

void TT_PageDigits(u16 page, u8* d)
{
	d[0] = 0;
	while (page >= 100) { page = (u16)(page - 100); d[0]++; }
	d[1] = 0;
	while (page >= 10) { page = (u16)(page - 10); d[1]++; }
	d[2] = (u8)page;
}

void TT_WaitTick(void)
{
	EnableInterrupt();
	Halt();
}

bool TT_TimedOut(u16 start, u16 ticks)
{
	return (u16)(TT_Jiffy() - start) >= ticks;
}

static u8 Lower(c8 c)
{
	if ((c >= 'A') && (c <= 'Z'))
		return (u8)(c + 32);
	return (u8)c;
}

static bool PrefixCI(const c8* s, const c8* pfx)
{
	while (*pfx)
	{
		if (Lower(*s++) != Lower(*pfx++))
			return FALSE;
	}
	return TRUE;
}

static bool ParseIPv4(const c8* s, char* ip)
{
	u8 part;
	u8 n;
	u16 acc;

	n = 0;
	acc = 0;
	part = 0;
	while (1)
	{
		c8 c = *s++;
		if ((c >= '0') && (c <= '9'))
		{
			acc = (u16)(acc * 10 + (u16)(c - '0'));
			if (acc > 255)
				return FALSE;
			part = 1;
		}
		else if ((c == '.') || (c == 0))
		{
			if (!part)
				return FALSE;
			ip[n++] = (char)acc;
			acc = 0;
			part = 0;
			if (c == 0)
				return (n == 4);
			if (n >= 4)
				return FALSE;
		}
		else
			return FALSE;
	}
}

static void SetDefaultHost(void)
{
#if TT_DIRECT_NOS
	Mem_Copy(TT_NOS_HOST, g_TTHost, (u16)(TT_NOS_HOST_LEN + 1));
	g_TTPort = TT_NOS_PORT;
#else
	Mem_Copy("127.0.0.1", g_TTHost, 10);
	g_TTPort = 8080;
#endif
}

#if !TT_DIRECT_NOS
/* host, host:port, or http://host[:port]. https:// rejected. */
static bool ParseHostPort(const c8* in)
{
	const c8* s;
	u8 hn;
	u16 port;
	u8 d;

	s = in;
	g_TTPort = 80;
	if (PrefixCI(s, "https://"))
		return FALSE;
	if (PrefixCI(s, "http://"))
		s += 7;

	if (!*s)
		return FALSE;

	hn = 0;
	while (*s && (*s != '/') && (*s != ':') && (*s != ' ') && (*s != '\t')
		&& (*s != '\r') && (*s != '\n'))
	{
		if (hn >= TT_HOST_MAX)
			return FALSE;
		g_TTHost[hn++] = *s++;
	}
	g_TTHost[hn] = 0;
	if (!hn)
		return FALSE;

	if (*s == ':')
	{
		s++;
		port = 0;
		if ((*s < '0') || (*s > '9'))
			return FALSE;
		while ((*s >= '0') && (*s <= '9'))
		{
			d = (u8)(*s - '0');
			if ((port > 6553) || ((port == 6553) && (d > 5)))
				return FALSE;
			port = (u16)(port * 10 + d);
			s++;
		}
		if (port == 0)
			return FALSE;
		g_TTPort = port;
	}
	return TRUE;
}

static void SkipSp(const c8** p)
{
	while ((**p == ' ') || (**p == '\t'))
		(*p)++;
}

/* One TT.CFG line: proxy-url=host[:port], or a bare host:port. refresh= ignored. */
static void ParseCfgLine(const c8* s)
{
	SkipSp(&s);
	if (!*s || (*s == '#') || (*s == ';'))
		return;
	if (PrefixCI(s, "refresh"))
		return;
	if (PrefixCI(s, "proxy-url"))
	{
		s += 9;
		SkipSp(&s);
		if (*s == '=')
			s++;
		SkipSp(&s);
		if (*s && !ParseHostPort(s))
			SetDefaultHost();
		return;
	}
	if ((*s != '=') && !PrefixCI(s, "proxy"))
		if (!ParseHostPort(s))
			SetDefaultHost();
}
#endif

void TT_LoadCfg(void)
{
	SetDefaultHost();
	g_RefreshSec = TT_REFRESH_DEFAULT;
#if TT_DIRECT_NOS
	/* Pico+: host/port/refresh are compiled in. TT.CFG is emulator-only. */
	return;
#else
	{
	u8 h;
	u16 n;
	u16 i;
	c8 buf[128];

	h = DOS_OpenHandle("TT.CFG", O_RDONLY);
	if (h == HANDLE_INVALID)
		return;
	n = DOS_ReadHandle(h, buf, (u16)(sizeof(buf) - 1));
	DOS_CloseHandle(h);
	if (n == 0)
		return;
	buf[n] = 0;
	i = 0;
	while (i < n)
	{
		u16 start;

		while ((i < n) && ((buf[i] == '\r') || (buf[i] == '\n')))
			i++;
		if (i >= n)
			break;
		start = i;
		while ((i < n) && (buf[i] != '\r') && (buf[i] != '\n'))
			i++;
		buf[i] = 0;
		ParseCfgLine(buf + start);
		i++;
	}
	}
#endif
}

static bool ResolveHost(void)
{
	tcpip_unapi_dns_q q;
	int err;
	u16 start;

	if (ParseIPv4(g_TTHost, g_Ip))
		return TRUE;

	/* Host name for dns_q: page-3 g_Request (Pico reads it with page 1 = BIOS). */
	{
		u8 i = 0;
		while (g_TTHost[i] && (i < TT_HOST_MAX))
		{
			g_Request[i] = g_TTHost[i];
			i++;
		}
		g_Request[i] = 0;
	}

	Mem_Set(0, &q, sizeof(q));
	q.flags = 0;
	err = tcpip_dns_q(g_Request, &q);
	TT_AfterUnapi();
	if (err != ERR_OK)
		return FALSE;
	if (q.state >= 1)
	{
		Mem_Copy(q.host_ip, g_Ip, 4);
		return TRUE;
	}
	start = TT_Jiffy();
	while (!TT_TimedOut(start, TT_TIMEOUT_TICKS))
	{
		q.flags = 0;
		err = tcpip_dns_s(&q);
		TT_AfterUnapi();
		if (err != ERR_OK)
			return FALSE;
		if (q.state == 2)
		{
			Mem_Copy(q.host_ip, g_Ip, 4);
			return TRUE;
		}
		TT_WaitTick();
	}
	return FALSE;
}

static bool TcpEstablished(int conn, u16 ticks)
{
	tcpip_unapi_tcp_conn_parms st;
	int err;
	u16 start;
	u8 tries;

	tries = 0;
	start = TT_Jiffy();
	while (!TT_TimedOut(start, ticks))
	{
		if (Cancelled())
			return FALSE;
		err = tcpip_tcp_state(conn, &st);
		TT_AfterUnapi();
		if (err == ERR_OK)
		{
			if (st.conn_state == TCP_STATE_ESTABLISHED)
				return TRUE;
			if (st.conn_state >= TCP_STATE_CLOSE_WAIT)
				return FALSE;
		}
		else if (err == ERR_NO_NETWORK)
		{
			if (tries++ >= 1)
				return FALSE;
		}
		else if (err == ERR_NO_CONN)
			return FALSE;
		TT_WaitTick();
	}
	return FALSE;
}

static bool OpenTcp(int* conn)
{
	tcpip_unapi_tcp_conn_parms tcp;
	int err;
	u8 att;
	u8 max_att;

	Mem_Set(0, &tcp, sizeof(tcp));
	Mem_Copy(g_Ip, tcp.dest_ip, 4);
	tcp.dest_port = (int)g_TTPort;
	tcp.local_port = (int)0xFFFF;
	tcp.user_timeout = 0;
	tcp.flags = 0;
#if TT_DIRECT_NOS
	/* UNAPI 1.1: TLS on, no cert verify. Hostname at +11 is page-3 g_Request. */
	{
		u8 i = 0;
		u16 hp;

		while (g_TTHost[i] && (i < TT_HOST_MAX))
		{
			g_Request[i] = g_TTHost[i];
			i++;
		}
		g_Request[i] = 0;
		tcp.flags = TT_TCP_FLAG_TLS;
		hp = (u16)g_Request;
		tcp.conn_state = (char)(hp & 0xFF);
		tcp.close_reason = (char)(hp >> 8);
	}
#endif

	/* unapinet loopback: a few short retries while the proxy wakes up. */
	max_att = ((u8)g_Ip[0] == 127) ? 4 : 1;
	for (att = 0; att < max_att; att++)
	{
		if (Cancelled())
			return FALSE;
		*conn = 0;
		err = tcpip_tcp_open(&tcp, conn);
		TT_AfterUnapi();
		if (err != ERR_OK)
		{
			if (att + 1 >= max_att)
				return FALSE;
			TT_WaitTick();
			continue;
		}
		if (TcpEstablished(*conn, (max_att > 1 && att + 1 < max_att) ? 40 : TT_TIMEOUT_TICKS))
			return TRUE;
		tcpip_tcp_abort(*conn);
		TT_AfterUnapi();
		TT_WaitTick();
	}
	return FALSE;
}

static u16 BodyOff(u16 len)
{
	u16 i;

	for (i = 0; i + 3 < len; i++)
	{
		if ((g_Http[i] == '\r') && (g_Http[i + 1] == '\n') &&
			(g_Http[i + 2] == '\r') && (g_Http[i + 3] == '\n'))
			return (u16)(i + 4);
		if ((g_Http[i] == '\n') && (g_Http[i + 1] == '\n'))
			return (u16)(i + 2);
	}
	return 0;
}

static u16 ContentLength(u16 hdr)
{
	u16 i;
	u16 v;

	for (i = 0; i + 16 < hdr; i++)
	{
		if (!PrefixCI(g_Http + i, "content-length:"))
			continue;
		i = (u16)(i + 15);
		while ((i < hdr) && ((g_Http[i] == ' ') || (g_Http[i] == '\t')))
			i++;
		v = 0;
		while ((i < hdr) && (g_Http[i] >= '0') && (g_Http[i] <= '9'))
		{
			if (v > 6553)
				return 0;
			v = (u16)(v * 10 + (u8)(g_Http[i] - '0'));
			i++;
		}
		return v;
	}
	return 0;
}

/* Stop on Content-Length or a short read — never wait for FIN. Halt only
 * when UNAPI has nothing; a full 255-byte chunk is followed by another recv
 * immediately (HGET/ducasp: no extra VDP tick). */
static u16 RecvAll(int conn)
{
	tcpip_unapi_tcp_conn_parms st;
	int err;
	u16 start;
	u16 total = 0;
	u16 n;
	u16 want;
	u16 off = 0;
	u16 need = 0;
	u8 i;

	/* unapinet loopback needs a couple of frames before the proxy reply
	 * is visible; skip this on a real host. */
	if ((u8)g_Ip[0] == 127)
	{
		for (i = 0; i < 3; i++)
			TT_WaitTick();
	}

	start = TT_Jiffy();
	while (total < TT_HTTP_MAX)
	{
		if (need && (total >= need))
			break;
		if (TT_TimedOut(start, TT_TIMEOUT_TICKS))
			break;
		if (Cancelled())
			break;

		want = 255;
		if (need && ((u16)(need - total) < want))
			want = (u16)(need - total);
		if (want == 0)
			break;

		Mem_Set(0, &st, sizeof(st));
		err = tcpip_tcp_rcv(conn, g_Chunk, (int)want, &st);
		TT_AfterUnapi();
		if (err == ERR_NO_DATA)
		{
			TT_WaitTick();
			continue;
		}
		if (err != ERR_OK)
			break;

		n = (u16)st.incoming_bytes;
		if (n > want)
			n = want;
		if (n == 0)
		{
			TT_WaitTick();
			continue;
		}
		if ((u16)(total + n) > TT_HTTP_MAX)
			return 0xFFFF;
		Mem_Copy(g_Chunk, g_Http + total, n);
		total = (u16)(total + n);
		start = TT_Jiffy();

		if (!off)
		{
			off = BodyOff(total);
			if (off)
			{
				u16 cl = ContentLength(off);
				if (cl)
					need = (u16)(off + cl);
			}
		}
		if (need)
		{
			if (total >= need)
				break;
		}
		else if (n < want)
			break; /* no Content-Length: a short read ends the body */
	}
	return total;
}

static u16 HttpStatus(u16 len)
{
	u16 i;
	u16 v;

	if (len < 12)
		return 0;
	i = 0;
	while ((i < len) && (g_Http[i] != ' '))
		i++;
	if ((i >= len) || (g_Http[i] != ' '))
		return 0;
	i++;
	v = 0;
	while ((i < len) && (g_Http[i] >= '0') && (g_Http[i] <= '9'))
	{
		v = (u16)(v * 10 + (u8)(g_Http[i] - '0'));
		i++;
	}
	return v;
}

u8 TT_InitNet(void)
{
#if !TT_ROM
	char* impl_name;
	int spec_ver;
	int impl_ver;
#endif
	int n;
#if !TT_ROM
	u16 i;
#endif

	EnableInterrupt();
	TT_Dbg(0x30);
	n = tcpip_enumerate();
	TT_Dbg(0x34);
	TT_AfterUnapi();
	TT_Dbg(0x35);
	if (n <= 0)
		return TT_ERR_UNAPI;
#if TT_ROM
	/* The name is not used by the cartridge, and UNAPI may destroy IY before
	 * the assembly name-copy path consumes it. Defer DNS until first fetch. */
	return TT_OK;
#else
	impl_name = 0;
	spec_ver = 0;
	impl_ver = 0;
	TT_Dbg(0x40);
	tcpip_impl_getinfo(&impl_name, &spec_ver, &impl_ver);
	TT_AfterUnapi();
	TT_Dbg(0x62);
	if (!ResolveHost())
		return TT_ERR_DNS;
	/* AUTOEXEC chains UNAPI then MSXTT with no pause. */
	for (i = 0; i < 50; i++)
		TT_WaitTick();
	return TT_OK;
#endif
}

u8 TT_FetchPage(u16 page, u8 sub)
{
	int conn;
	int err;
	c8* p;
	u8 d0, d1, d2;
	u16 got;
	u16 st;
	u16 off;
	u16 body;

	if (page > 999)
		return TT_ERR_HTTP;

	g_Cancel = 0;
	EnableInterrupt();
#if TT_ROM
	if (!ResolveHost())
		return g_Cancel ? TT_ERR_CANCEL : TT_ERR_DNS;
#endif
	if (!OpenTcp(&conn))
		return g_Cancel ? TT_ERR_CANCEL : TT_ERR_TCP;

	/* Avoid SDCC div helpers for three digits. */
	{
		u8 dig[3];
		TT_PageDigits(page, dig);
		d0 = dig[0];
		d1 = dig[1];
		d2 = dig[2];
	}

	p = g_Request;
	Mem_Copy("GET /json/", p, 10);
	p += 10;
	*p++ = (c8)('0' + d0);
	*p++ = (c8)('0' + d1);
	*p++ = (c8)('0' + d2);
	/* The first subpage is the bare id; later ones are NNN-S. */
	if (sub >= 2)
	{
		u8 s = sub;
		*p++ = '-';
		if (s >= 10)
		{
			u8 t = 0;
			while (s >= 10) { s = (u8)(s - 10); t++; }
			*p++ = (c8)('0' + t);
		}
		*p++ = (c8)('0' + s);
	}
	Mem_Copy(" HTTP/1.0\r\nHost: ", p, 17);
	p += 17;
	{
		u8 i = 0;
		while (g_TTHost[i] && (p < g_Request + sizeof(g_Request) - 27))
			*p++ = g_TTHost[i++];
	}
	Mem_Copy("\r\nConnection: close\r\n\r\n", p, 23);

	err = tcpip_tcp_send(conn, g_Request, (int)(p + 23 - g_Request), 1);
	TT_AfterUnapi();
	if (err != ERR_OK)
	{
		tcpip_tcp_abort(conn);
		TT_AfterUnapi();
		return TT_ERR_TCP;
	}

	got = RecvAll(conn);
	/* Always release the connection: the body is already in g_Http, and UNAPI
	 * has only a handful of slots. Abort rather than close so the slot is free
	 * at once instead of lingering in TIME_WAIT. */
	tcpip_tcp_abort(conn);
	TT_AfterUnapi();

	if (g_Cancel)
		return TT_ERR_CANCEL;
	if (got == 0xFFFF)
		return TT_ERR_SIZE;
	if (got == 0)
		return TT_ERR_TCP;

	st = HttpStatus(got);
	if (st != 200)
		return TT_ERR_HTTP;
	off = BodyOff(got);
	if (!off || (off >= got))
		return TT_ERR_PARSE;
	body = (u16)(got - off);
	/* Navigation first, so the links survive a content parse failure. */
	TT_ParseNav(g_Http + off, body);
	if (!TT_ParseHttpJson(g_Http + off, body))
		return TT_ERR_PARSE;
	return TT_OK;
}
