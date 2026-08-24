#!/usr/bin/env python3
"""Matrix API — tank (pred/prey) + war (army) live ESP / twin."""
from __future__ import annotations

import html as html_mod
import json
import os
import sys
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs
import urllib.parse
import urllib.request

MATRIX = os.environ.get("MATRIX_URL", "http://rage-matrix.local").rstrip("/")
TIMEOUT = 1.2
_HERE = Path(__file__).resolve().parent
_CANDIDATES = (
    Path(os.environ["MATRIX_ROOT"]) if os.environ.get("MATRIX_ROOT") else None,
    Path.home() / "Documents/the-grid/maker-led-display",
    _HERE.parent,
    _HERE.parent.parent,
)
SIM_ROOT = next((p for p in _CANDIDATES if p and (p / "sim").is_dir()), _HERE.parent)
if str(SIM_ROOT) not in sys.path:
    sys.path.insert(0, str(SIM_ROOT))

_view = "tank"
_tank = None
_war = None

TANK_CMDS = {"feed", "scatter", "shake", "seed", "follow", "pan", "hunt", "focus"}
WAR_CMDS = {"next", "wall", "west", "east", "focus", "pan", "seed", "civ", "brawl", "epic", "opts", "age"}
MODES = (
    "tank", "war", "wave", "fire", "plasma", "life", "stars", "clock",
    "rain", "tunnel", "bars", "pulse", "sine", "bounce", "spark",
    "lissa", "ca", "xor", "rings", "cycle", "grid", "disc", "text",
)


def _tank_get():
    global _tank
    if _tank is None:
        from sim.critters import Tank

        _tank = Tank()
        _tank.seed()
    _tank.step()
    return _tank


def _war_get():
    global _war
    if _war is None:
        from sim.war import War

        _war = War()
        _war.seed()
    _war.step()
    return _war


def _rust():
    try:
        from sim import rust_twin

        return rust_twin if rust_twin.available() else None
    except Exception:
        return None


def _http(method: str, path: str, data: dict | None = None) -> dict:
    url = MATRIX + path
    body = None
    headers = {}
    if method == "POST":
        body = urllib.parse.urlencode(data or {}).encode()
        headers["Content-Type"] = "application/x-www-form-urlencoded"
    req = urllib.request.Request(url, data=body, method=method, headers=headers)
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        raw = r.read().decode("utf-8", errors="replace")
    return json.loads(raw)


def set_view(name: str) -> str:
    global _view
    if name:
        _view = name
    return _view


def pull_fb(view: str | None = None) -> tuple[dict, str]:
    v = view or _view
    try:
        st = _http("GET", "/panel.json")
        mode = st.get("mode") or st.get("sim") or v
        extra = {}
        try:
            if mode == "war":
                extra = _http("GET", "/war.json")
            elif mode == "tank":
                extra = _http("GET", "/tank.json")
        except Exception:
            extra = {}
        extra.update(st)
        extra["source"] = "live"
        extra["sim"] = mode
        return extra, extra.get("px") or ""
    except Exception:
        rust = _rust()
        if rust is not None:
            try:
                return rust.pull(v)
            except Exception:
                pass
        if v == "war":
            w = _war_get()
            return w.state(), w.px_hex()
        t = _tank_get()
        return t.state(), t.px_hex()


def cmd(name: str, data: dict | None = None, view: str | None = None) -> dict:
    v = view or _view
    data = data or {}
    try:
        if name == "view":
            set_view(str(data.get("sim") or data.get("view") or v))
            path = "/war/focus" if _view == "war" else "/tank/focus"
            try:
                _http("POST", "/mode", {"name": _view})
            except Exception:
                pass
            st = _http("POST", path, {})
            st["source"] = "live"
            st["sim"] = _view
            return st
        if name == "mode":
            want = str(data.get("name") or data.get("sim") or v)
            set_view("war" if want == "war" else "tank" if want == "tank" else want)
            st = _http("POST", "/mode", {"name": want})
            st["source"] = "live"
            st["sim"] = want
            return st
        prefix = "/war" if v == "war" else "/tank"
        st = _http("POST", f"{prefix}/{name}", data)
        st["source"] = "live"
        st["sim"] = v
        return st
    except Exception as e:
        rust = _rust()
        if rust is not None:
            try:
                st = rust.cmd(v, name, data)
                st["fallback"] = str(e)
                return st
            except Exception:
                pass
        if v == "war":
            w = _war_get()
            if name == "seed":
                w.seed()
            elif name == "next":
                w.rematch()
            elif name == "wall":
                w.drop_wall(int(data.get("x") or 16), int(data.get("y") or 4))
            elif name == "west":
                w.reinforce(0, str(data.get("arty", "0")) not in ("0", "false", ""))
            elif name == "east":
                w.reinforce(1, str(data.get("arty", "0")) not in ("0", "false", ""))
            st = w.state()
        else:
            t = _tank_get()
            if name == "feed":
                x, y = data.get("x"), data.get("y")
                t.feed(int(x) if x not in (None, "") else None, int(y) if y not in (None, "") else None)
            elif name == "scatter":
                t.scatter()
            elif name == "shake":
                t.do_shake(float(data.get("amp") or 1.2))
            elif name == "seed":
                t.seed()
            elif name == "hunt":
                t.drop_hunter()
            elif name == "follow":
                t.follow = str(data.get("on", "1")) not in ("0", "false")
            elif name == "pan":
                t.follow = False
                t.cam_x = (t.cam_x + int(data.get("dx") or 0)) % 64
                t.cam_y = (t.cam_y + int(data.get("dy") or 0)) % 18
            st = t.state()
        st["fallback"] = str(e)
        return st


def board_svg(px: str, w: int = 32, h: int = 9) -> str:
    cells = []
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 2
            try:
                val = int(px[i : i + 2], 16) if px and i + 2 <= len(px) else 0
            except ValueError:
                val = 0
            g = 10 + int(val * 0.92)
            b = 14 + int(val * 1.05)
            r = 8 + int(val * 0.78)
            cells.append(
                f'<rect x="{x}" y="{y}" width="1" height="1" rx="0.18" '
                f'fill="rgb({min(255,r)},{min(255,g)},{min(255,b)})"/>'
            )
    return (
        f'<svg class="tank-svg" viewBox="0 0 {w} {h}" width="100%" '
        f'preserveAspectRatio="xMidYMid meet" role="img" aria-label="32 by 9 view">'
        f'<rect width="{w}" height="{h}" fill="#0b0d12"/>'
        + "".join(cells)
        + "</svg>"
    )


def board_html() -> str:
    st, px = pull_fb()
    src = st.get("source") or "twin"
    src_cls = "pill-ok" if src == "live" else "pill-warn"
    sim = st.get("sim") or _view
    if sim == "war":
        note = "CAMP → MELEE → GUNS → ARTY. Score card then NEXT. Hold = wait for Next match."
        kpis = f"""
    <div class="kpi"><div class="muted">west</div><strong>{st.get("west", 0)}</strong></div>
    <div class="kpi"><div class="muted">east</div><strong>{st.get("east", 0)}</strong></div>
    <div class="kpi"><div class="muted">score</div><strong>{st.get("kw", 0)}–{st.get("ke", 0)}</strong></div>
    <div class="kpi"><div class="muted">age</div><strong>{html_mod.escape(str(st.get("age_name") or "—"))}</strong></div>
    <div class="kpi"><div class="muted">match</div><strong>{st.get("match", 0)}</strong></div>"""
    elif sim == "tank":
        note = "tadpole=prey · chevron=hunter · ADC=LiPo · no IMU/LDR"
        kpis = f"""
    <div class="kpi"><div class="muted">prey</div><strong>{st.get("prey", 0)}</strong></div>
    <div class="kpi"><div class="muted">hunters</div><strong>{st.get("pred", 0)}</strong></div>
    <div class="kpi"><div class="muted">gen</div><strong>{st.get("gen", 0)}</strong></div>
    <div class="kpi"><div class="muted">births</div><strong>{st.get("births", 0)}</strong></div>"""
    else:
        note = "reel mode — title card then the visual. 32×9 is the camera."
        kpis = f"""
    <div class="kpi"><div class="muted">mode</div><strong>{html_mod.escape(str(sim))}</strong></div>"""
    return f"""<div id="tank-board" class="tank-board"
  hx-get="/api/matrix/v1/board" hx-trigger="every 450ms" hx-swap="outerHTML">
  <div class="kpis">{kpis}
    <div class="kpi"><div class="muted">{html_mod.escape(sim)}</div><span class="pill {src_cls}">{html_mod.escape(src)}</span></div>
  </div>
  {board_svg(px)}
  <p class="muted">{html_mod.escape(note)}</p>
</div>"""


def _parse(raw: bytes, headers: dict[str, str]) -> dict[str, Any]:
    text = (raw or b"").decode("utf-8", errors="replace")
    if not text.strip():
        return {}
    ctype = (headers.get("Content-Type") or "").lower()
    if "json" in ctype:
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return {}
    return {k: (v[-1] if v else "") for k, v in parse_qs(text, keep_blank_values=True).items()}


def handle_get(path: str, qs: dict) -> tuple | None:
    if path in ("/api/matrix/v1/health", "/api/matrix/v1/health/"):
        st, _ = pull_fb()
        mode = {}
        try:
            mode = _http("GET", "/mode.json")
        except Exception:
            pass
        return 200, {"ok": True, "live": st.get("source") == "live", "imu": False, "ldr": False, "mode": mode.get("mode"), "title": mode.get("title"), **st}
    if path in ("/api/matrix/v1/state", "/api/matrix/v1/state/"):
        st, _ = pull_fb()
        return 200, st
    if path in ("/api/matrix/v1/board", "/api/matrix/v1/board/"):
        return 200, board_html().encode("utf-8"), "text/html; charset=utf-8"
    return None


def handle_post(path: str, raw: bytes, headers: dict[str, str]) -> tuple | None:
    data = _parse(raw, headers)
    name = path.rstrip("/").rsplit("/", 1)[-1]
    war_path = "/war/" in path or path.rstrip("/").endswith("/war")
    if name == "view":
        set_view(str(data.get("sim") or data.get("view") or "tank"))
        cmd("view", {"sim": _view})
        return 200, board_html().encode("utf-8"), "text/html; charset=utf-8"
    if name == "mode":
        want = str(data.get("name") or data.get("sim") or "tank")
        set_view("war" if want == "war" else "tank" if want == "tank" else want)
        cmd("mode", {"name": want})
        return 200, board_html().encode("utf-8"), "text/html; charset=utf-8"
    if war_path or name in {"next", "wall", "west", "east"}:
        set_view("war")
        cmd(name if name != "war" else "focus", data, "war")
        return 200, board_html().encode("utf-8"), "text/html; charset=utf-8"
    if name in TANK_CMDS or "/tank/" in path:
        set_view("tank")
        cmd(name if name != "tank" else "focus", data, "tank")
        return 200, board_html().encode("utf-8"), "text/html; charset=utf-8"
    return None
