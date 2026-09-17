#!/usr/bin/env python3
"""HTTP→HTTPS proxy for NOS Teletekst JSON (openMSXnet has no TLS)."""
from __future__ import annotations

import re
import sys
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BIND = "0.0.0.0"
PORT = 8080
UPSTREAM = "https://teletekst-data.nos.nl"
UA = "MSX2TT-proxy/1"
TIMEOUT = 10
PAGE_RE = re.compile(r"^/json/(\d{3}(?:-\d+)?)$")


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

		url = "%s/json/%s" % (UPSTREAM, m.group(1))
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
				self.wfile.write(view[off:off + n])
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
	print("Teletekst proxy http://127.0.0.1:%d/json/{page} (listen %s) -> %s/json/{page}" % (port, BIND, UPSTREAM), flush=True)
	try:
		httpd.serve_forever()
	except KeyboardInterrupt:
		print("\nStopped")
	return 0


if __name__ == "__main__":
	sys.exit(main())
