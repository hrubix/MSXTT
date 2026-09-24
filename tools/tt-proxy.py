#!/usr/bin/env python3
"""HTTP→HTTPS proxy for NOS Teletekst JSON, plus local override pages.

Local Level-1 raw fixtures under tools/tt-pages/ (e.g. 899.json) are converted
to NOS-style HTML JSON so MSXTT's existing parser can render them. Everything
else is proxied to teletekst-data.nos.nl.
"""
from __future__ import annotations

import json
import re
import sys
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

BIND = "0.0.0.0"
PORT = 8080
UPSTREAM = "https://teletekst-data.nos.nl"
UA = "MSX2TT-proxy/1"
TIMEOUT = 10
PAGE_RE = re.compile(r"^/json/(\d{3})(?:-(\d+))?$")
PAGES_DIR = Path(__file__).resolve().parent / "tt-pages"

COLOUR = (
	"black",
	"red",
	"green",
	"yellow",
	"blue",
	"magenta",
	"cyan",
	"white",
)

# page_id -> { "1": nos_json_dict, "2": ..., ... }
LOCAL_PAGES: dict[str, dict[str, dict]] = {}


def _esc_text(s: str) -> str:
	return (
		s.replace("&", "&amp;")
		.replace("<", "&lt;")
		.replace(">", "&gt;")
		.replace('"', "&quot;")
	)


def _span(text: str, fg: int, bg: int) -> str:
	if not text:
		return ""
	cls = COLOUR[fg & 7]
	if bg:
		cls = "%s bg-%s" % (cls, COLOUR[bg & 7])
	return '<span class="%s ">%s</span>' % (cls, text)


def row_hex_to_nos_html(row_hex: str) -> str:
	"""Convert 40 raw Level-1 Teletext bytes to one NOS HTML content line."""
	raw = bytes.fromhex(row_hex)
	if len(raw) != 40:
		raise ValueError("row must be 40 bytes, got %d" % len(raw))

	fg = 7
	bg = 0
	graphics = False
	hold = False
	held = 0x20
	conceal = False

	parts: list[str] = []
	buf: list[str] = []
	cur_fg, cur_bg = fg, bg

	def flush() -> None:
		nonlocal buf, cur_fg, cur_bg
		if not buf:
			return
		parts.append(_span("".join(buf), cur_fg, cur_bg))
		buf = []

	def ensure_style() -> None:
		nonlocal buf, cur_fg, cur_bg
		if cur_fg != fg or cur_bg != bg:
			flush()
			cur_fg, cur_bg = fg, bg

	def emit_space() -> None:
		ensure_style()
		buf.append(" ")

	def emit_char(ch: int) -> None:
		nonlocal held
		ensure_style()
		if conceal:
			buf.append(" ")
			return
		if graphics:
			if (0x20 <= ch <= 0x3F) or (0x60 <= ch <= 0x7F):
				held = ch
				buf.append("&#xF0%02X;" % ch)
			elif hold and held != 0x20:
				buf.append("&#xF0%02X;" % held)
			else:
				buf.append(" ")
		elif ch == 0x7F:
			buf.append("&#xF07F;")
		elif 0x20 <= ch <= 0x7E:
			buf.append(_esc_text(chr(ch)))
		else:
			buf.append(" ")

	for b in raw:
		if b < 0x20:
			if b == 0x1D:
				# New background is set-at: bg becomes current fg, then space.
				bg = fg
				emit_space()
			else:
				emit_space()
				if b <= 0x07:
					fg = b
					graphics = False
					hold = False
					conceal = False
				elif 0x10 <= b <= 0x17:
					fg = b & 7
					graphics = True
					conceal = False
				elif b == 0x18:
					conceal = True
				elif b == 0x1C:
					bg = 0
				elif b == 0x1E:
					hold = True
				elif b == 0x1F:
					hold = False
					held = 0x20
				# 08 flash, 09 steady, 0A/0B box, 0C–0F size, 19/1A mosaic
				# style, 1B ESC: spacing only (MSXTT has no effect).
		else:
			emit_char(b)

	flush()
	return "".join(parts)


def raw_subpage_to_nos(
	page: int,
	sub: int,
	rows_hex: list[str],
	sub_count: int,
) -> dict:
	lines = [row_hex_to_nos_html(h) for h in rows_hex]
	# MSXTT has 25 rows; raw fixture has 24 display rows.
	while len(lines) < 24:
		lines.append(_span(" " * 40, 7, 0))
	content = "<br/>\n".join(lines) + "<br/>\n"

	def sub_id(n: int) -> str:
		return str(page) if n <= 1 else "%d-%d" % (page, n)

	prev_sub = "" if sub <= 1 else sub_id(sub - 1)
	next_sub = "" if sub >= sub_count else sub_id(sub + 1)

	fast = []
	for i in range(1, min(5, sub_count + 1)):
		fast.append({"title": "%d/%d" % (page, i), "page": sub_id(i)})

	return {
		"prevPage": "",
		"nextPage": "100",
		"prevSubPage": prev_sub,
		"nextSubPage": next_sub,
		"fastTextLinks": fast,
		"content": content,
	}


def load_local_pages() -> None:
	LOCAL_PAGES.clear()
	if not PAGES_DIR.is_dir():
		return
	for path in sorted(PAGES_DIR.glob("*.json")):
		try:
			data = json.loads(path.read_text(encoding="utf-8"))
		except (OSError, json.JSONDecodeError) as exc:
			sys.stderr.write("skip %s: %s\n" % (path.name, exc))
			continue
		if data.get("format") != "teletext-level1-raw-rows":
			sys.stderr.write("skip %s: unknown format\n" % path.name)
			continue
		page = int(data["page"])
		subs = data.get("subpages") or []
		n = len(subs)
		bucket: dict[str, dict] = {}
		for sp in subs:
			sub_n = int(sp["subpage"])
			nos = raw_subpage_to_nos(page, sub_n, sp["rows_hex"], n)
			key = "1" if sub_n == 1 else str(sub_n)
			bucket[key] = nos
			# Also expose explicit "-1" alias
			if sub_n == 1:
				bucket["1"] = nos
		LOCAL_PAGES[str(page)] = bucket
		sys.stderr.write(
			"local page %d: %d subpage(s) from %s\n" % (page, n, path.name)
		)


def lookup_local(page: str, sub: str | None) -> dict | None:
	bucket = LOCAL_PAGES.get(page)
	if not bucket:
		return None
	if not sub or sub == "0" or sub == "1":
		return bucket.get("1")
	return bucket.get(sub)


class Handler(BaseHTTPRequestHandler):
	protocol_version = "HTTP/1.0"

	def log_message(self, fmt: str, *args) -> None:
		sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

	def do_GET(self) -> None:
		path = self.path.split("?", 1)[0]
		m = PAGE_RE.match(path)
		if not m:
			self._send(400, b"Bad path\n", "text/plain")
			return

		page, sub = m.group(1), m.group(2)
		local = lookup_local(page, sub)
		if local is not None:
			body = json.dumps(local, ensure_ascii=False, separators=(",", ":")).encode(
				"utf-8"
			)
			self._send(200, body, "application/json; charset=utf-8")
			return

		suffix = page if not sub else "%s-%s" % (page, sub)
		url = "%s/json/%s" % (UPSTREAM, suffix)
		req = urllib.request.Request(url, headers={"User-Agent": UA})
		try:
			with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
				body = resp.read()
				status = resp.status
				ctype = resp.headers.get("Content-Type", "application/json")
		except urllib.error.HTTPError as exc:
			body = exc.read()
			status = exc.code
			ctype = "application/json"
			if exc.headers:
				ctype = exc.headers.get("Content-Type", ctype)
		except Exception as exc:
			self._send(502, ("Upstream error: %s\n" % exc).encode("utf-8"), "text/plain")
			return

		self._send(status, body, ctype)

	def do_HEAD(self) -> None:
		self._send(400, b"GET only\n", "text/plain")

	def _send(self, status: int, body: bytes, content_type: str) -> None:
		self.send_response(status)
		self.send_header("Content-Type", content_type)
		self.send_header("Content-Length", str(len(body)))
		self.send_header("Connection", "close")
		self.end_headers()
		if self.command == "HEAD":
			return
		try:
			self.wfile.flush()
			view = memoryview(body)
			off = 0
			while off < len(body):
				n = min(512, len(body) - off)
				self.wfile.write(view[off : off + n])
				self.wfile.flush()
				off += n
		except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
			return


def main() -> int:
	port = PORT
	args = sys.argv[1:]
	i = 0
	while i < len(args):
		if args[i] in ("-p", "--port") and (i + 1) < len(args):
			port = int(args[i + 1])
			i += 2
			continue
		sys.stderr.write("Usage: tt-proxy.py [--port N]\n")
		return 2

	load_local_pages()

	ThreadingHTTPServer.allow_reuse_address = True
	try:
		httpd = ThreadingHTTPServer((BIND, port), Handler)
	except OSError as exc:
		sys.stderr.write(
			"Cannot bind %s:%d (%s).\n"
			"Another tt-proxy (or app) is still using that port.\n"
			"Re-run tools\\run-tt-proxy.bat — it frees 8080 first — or pass --port N.\n"
			% (BIND, port, exc)
		)
		return 1
	local_list = ", ".join(sorted(LOCAL_PAGES.keys())) or "(none)"
	print(
		"Teletekst proxy http://127.0.0.1:%d/json/{{page}} (listen %s) -> %s"
		% (port, BIND, UPSTREAM),
		flush=True,
	)
	print("Local overrides: %s (from %s)" % (local_list, PAGES_DIR), flush=True)
	try:
		httpd.serve_forever()
	except KeyboardInterrupt:
		print("\nStopped")
	return 0


if __name__ == "__main__":
	sys.exit(main())
