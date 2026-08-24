#!/usr/bin/env python3
"""Standalone Matrix desk. Proxies the ESP and falls back to the host twin."""
from __future__ import annotations

import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
import sys

if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import matrix_api  # noqa: E402
from compose import compose  # noqa: E402


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send(self, code, body, ctype="application/json"):
        if isinstance(body, (dict, list)):
            raw = json.dumps(body).encode()
            ctype = "application/json"
        elif isinstance(body, str):
            raw = body.encode()
        else:
            raw = body or b""
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        path = urlparse(self.path).path
        if path in ("/", "/index.html"):
            compose()
            html = (HERE / "static" / "index.html").read_bytes()
            return self._send(200, html, "text/html; charset=utf-8")
        if path.startswith("/api/matrix/v1/"):
            hit = matrix_api.handle_get(path, {})
            if hit is None:
                return self._send(404, {"ok": False})
            if len(hit) == 3:
                return self._send(hit[0], hit[1], hit[2])
            return self._send(hit[0], hit[1])
        return self._send(404, b"not found", "text/plain")

    def do_POST(self):
        path = urlparse(self.path).path
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n) if n else b""
        headers = {k: v for k, v in self.headers.items()}
        hit = matrix_api.handle_post(path, raw, headers)
        if hit is None:
            return self._send(404, {"ok": False})
        if len(hit) == 3:
            return self._send(hit[0], hit[1], hit[2])
        return self._send(hit[0], hit[1])


def main():
    compose()
    addr = ("127.0.0.1", int(__import__("os").environ.get("MATRIX_HUB_PORT", "8765")))
    httpd = ThreadingHTTPServer(addr, Handler)
    print(f"matrix hub http://{addr[0]}:{addr[1]}  board={matrix_api.MATRIX}")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
