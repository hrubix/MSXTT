#!/usr/bin/env python3
"""Audit NOS Teletekst JSON pages for characters MSXTT's parser supports.

Mirrors src/tt/parse.c EmitCodepoint / named-entity / UTF-8 C2–C3 handling.
Does not judge glyph appearance — only whether the parser would emit '?' .

Usage:
  python tools/check-nos-chars.py
  python tools/check-nos-chars.py --sweep          # also GET every 100..899 (many 404s)
  python tools/check-nos-chars.py --out docs/nos-char-audit.txt
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
import urllib.error
import urllib.request
from collections import defaultdict
from html import unescape
from html.parser import HTMLParser
from typing import Iterable

BASE = "https://teletekst-data.nos.nl/json/"
UA = "MSXTT-char-audit/1"
DELAY_S = 1.0

# Named entities accepted by parse.c (g_LatEnt + HTML basics).
LAT_ENT = {
	"egrave",
	"eacute",
	"ecirc",
	"euml",
	"agrave",
	"aacute",
	"auml",
	"igrave",
	"iacute",
	"iuml",
	"ograve",
	"oacute",
	"ouml",
	"ugrave",
	"uacute",
	"uuml",
	"ccedil",
	"ntilde",
	"Egrave",
	"Eacute",
	"Euml",
	"Auml",
	"Iuml",
	"Ouml",
	"Uuml",
	"Ccedil",
	"nbsp",
	"amp",
	"lt",
	"gt",
	"quot",
}

PAGE_ID_RE = re.compile(r"^(\d{3})(?:-(\d+))?$")
ENTITY_RE = re.compile(r"&(#x?[0-9A-Fa-f]+|#\d+|[A-Za-z]+);")


def is_supported_codepoint(cp: int) -> bool:
	"""True if parse.c EmitCodepoint / MapAscii path keeps a real glyph."""
	if cp == 0xA0:
		return True
	if 0x20 <= cp <= 0x7E:
		return True
	if 0xF000 <= cp <= 0xF07F:
		return True
	if 0xC0 <= cp <= 0xFF:
		return True
	# Control / DEL / C1 / etc. → '?' or space mapping; treat non-print as fail
	# only when they appear as content characters (rare).
	return False


def is_supported_named(name: str) -> bool:
	return name in LAT_ENT


def classify_entity(body: str) -> tuple[str, bool]:
	"""Return (label, supported). label is named=… or U+XXXX."""
	if body.startswith("#x") or body.startswith("#X"):
		try:
			cp = int(body[2:], 16)
		except ValueError:
			return f"entity=&{body};", False
		ok = is_supported_codepoint(cp)
		return f"U+{cp:04X}", ok
	if body.startswith("#"):
		try:
			cp = int(body[1:], 10)
		except ValueError:
			return f"entity=&{body};", False
		ok = is_supported_codepoint(cp)
		return f"U+{cp:04X}", ok
	ok = is_supported_named(body)
	return f"named={body}", ok


class TextCollector(HTMLParser):
	"""Collect character data; ignore tags except for entity-bearing text."""

	def __init__(self) -> None:
		super().__init__(convert_charrefs=False)
		self.chunks: list[str] = []

	def handle_data(self, data: str) -> None:
		self.chunks.append(data)

	def handle_entityref(self, name: str) -> None:
		self.chunks.append(f"&{name};")

	def handle_charref(self, name: str) -> None:
		self.chunks.append(f"&#{name};")


def analyze_content(content: str) -> set[str]:
	"""Return set of unsupported labels found in HTML content."""
	bad: set[str] = set()

	# Entities as written in the JSON (before browser unescape).
	for m in ENTITY_RE.finditer(content):
		label, ok = classify_entity(m.group(1))
		if not ok:
			bad.add(label)

	# Visible text codepoints (UTF-8 in JSON string after entity decode of text).
	parser = TextCollector()
	try:
		parser.feed(content)
		parser.close()
	except Exception:
		# Fall back: strip tags crudely.
		text = re.sub(r"<[^>]+>", "", content)
		parser.chunks = [text]

	raw_text = "".join(parser.chunks)
	# Decode numeric/named entities that HTMLParser left as literals in data,
	# and also expand &…; we already classified — for UTF-8 chars use unescape
	# on a copy with entities turned into chars for codepoint scan.
	expanded = unescape(re.sub(r"<[^>]+>", "", content))
	for ch in expanded:
		cp = ord(ch)
		if cp < 128:
			if cp < 0x20 and cp not in (0x09, 0x0A, 0x0D):
				bad.add(f"U+{cp:04X}")
			continue
		if not is_supported_codepoint(cp):
			bad.add(f"U+{cp:04X}")

	# Also scan raw_text for leftover &entity; not expanded
	for m in ENTITY_RE.finditer(raw_text):
		label, ok = classify_entity(m.group(1))
		if not ok:
			bad.add(label)

	return bad


def fetch_page(page_id: str, retries: int = 3) -> tuple[int, dict | None]:
	"""GET one page; retry on 429/503 with extra wait."""
	last_status = 0
	for attempt in range(retries):
		req = urllib.request.Request(BASE + page_id, headers={"User-Agent": UA})
		try:
			with urllib.request.urlopen(req, timeout=30) as resp:
				return resp.status, json.load(resp)
		except urllib.error.HTTPError as exc:
			last_status = exc.code
			if exc.code in (429, 503) and attempt + 1 < retries:
				time.sleep(2.0 * (attempt + 1))
				continue
			return exc.code, None
		except Exception:
			last_status = 0
			if attempt + 1 < retries:
				time.sleep(2.0 * (attempt + 1))
				continue
			return 0, None
	return last_status, None


def enqueue_links(data: dict, queue: list[str], seen: set[str]) -> None:
	"""Discover further pages from JSON nextPage / nextSubPage only."""

	def add(pid: str | None) -> None:
		if not pid:
			return
		pid = str(pid).strip()
		if not PAGE_ID_RE.match(pid):
			return
		if pid not in seen and pid not in queue:
			queue.append(pid)

	add(data.get("nextPage") or "")
	add(data.get("nextSubPage") or "")


def main(argv: Iterable[str] | None = None) -> int:
	ap = argparse.ArgumentParser(description="NOS JSON character support audit for MSXTT")
	ap.add_argument(
		"--sweep",
		action="store_true",
		default=False,
		help="also GET every bare page 100..899 (many 404s; not recommended)",
	)
	ap.add_argument(
		"--start",
		default="100",
		help="seed page id (default 100); walk nextPage/nextSubPage from here",
	)
	ap.add_argument("--out", type=str, default="", help="write report to this file")
	ap.add_argument("--delay", type=float, default=DELAY_S, help="delay between requests (default 1s)")
	args = ap.parse_args(list(argv) if argv is not None else None)

	queue: list[str] = [args.start]
	seen: set[str] = set()
	if args.sweep:
		for n in range(100, 900):
			pid = f"{n:03d}"
			if pid not in queue:
				queue.append(pid)

	lines: list[str] = []
	n_ok = n_bad = n_404 = n_err = 0
	global_bad: dict[str, list[str]] = defaultdict(list)

	def emit(s: str) -> None:
		print(s, flush=True)
		lines.append(s)

	emit("# MSXTT NOS character audit")
	emit(f"# source {BASE}  start={args.start}  sweep={args.sweep}  delay={args.delay}s")
	emit("# discovery: nextPage + nextSubPage")
	emit("")

	while queue:
		page_id = queue.pop(0)
		if page_id in seen:
			continue
		seen.add(page_id)

		status, data = fetch_page(page_id)
		time.sleep(args.delay)

		if status == 404 or (status >= 400 and data is None and status != 0):
			if status == 404:
				emit(f"{page_id:<12} 404")
				n_404 += 1
			else:
				emit(f"{page_id:<12} ERROR HTTP {status}")
				n_err += 1
			continue
		if data is None:
			emit(f"{page_id:<12} ERROR")
			n_err += 1
			continue

		enqueue_links(data, queue, seen)

		bad = analyze_content(data.get("content") or "")
		if not bad:
			emit(f"{page_id:<12} OK")
			n_ok += 1
		else:
			detail = "; ".join(sorted(bad))
			emit(f"{page_id:<12} UNSUPPORTED: {detail}")
			n_bad += 1
			for label in bad:
				if page_id not in global_bad[label] and len(global_bad[label]) < 5:
					global_bad[label].append(page_id)

	emit("")
	emit("=== SUMMARY ===")
	emit(f"pages checked: {len(seen)}")
	emit(f"OK:           {n_ok}")
	emit(f"UNSUPPORTED:  {n_bad}")
	emit(f"404:          {n_404}")
	emit(f"ERROR:        {n_err}")
	if global_bad:
		emit("")
		emit("=== UNSUPPORTED ITEMS (sample pages) ===")
		for label in sorted(global_bad):
			emit(f"  {label}: {', '.join(global_bad[label])}")
	else:
		emit("")
		emit("No unsupported characters found on live pages.")

	if args.out:
		from pathlib import Path

		path = Path(args.out)
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text("\n".join(lines) + "\n", encoding="utf-8")
		print(f"Wrote {path}", flush=True)

	return 1 if n_bad else 0


if __name__ == "__main__":
	sys.exit(main())
