#!/usr/bin/env python3
"""Write the standalone Matrix desk HTML."""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
for p in (HERE, ROOT):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

import matrix_api  # noqa: E402

OUT = HERE / "static" / "index.html"


def _mode_btns() -> str:
    bits = []
    for name in matrix_api.MODES:
        label = "WAR ZONE" if name == "war" else name.upper()
        bits.append(
            f'<button hx-post="/api/matrix/v1/mode" '
            f'hx-vals=\'{{"name":"{name}"}}\' '
            f'hx-target="#tank-board" hx-swap="outerHTML">{label}</button>'
        )
    return "\n    ".join(bits)


def _feed_grid() -> str:
    bits = []
    for y in range(3):
        for x in range(8):
            vx, vy = x * 4 + 2, y * 3 + 1
            bits.append(
                f'<button hx-post="/api/matrix/v1/tank/feed" '
                f'hx-vals=\'{{"x":"{vx}","y":"{vy}"}}\' '
                f'hx-target="#tank-board" hx-swap="outerHTML">{x},{y}</button>'
            )
    return "\n    ".join(bits)


def compose() -> Path:
    board = matrix_api.board_html()
    html = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>Rage Matrix</title>
<script src="https://unpkg.com/htmx.org@2.0.4"></script>
<style>
  :root {{ color-scheme: dark; }}
  body {{ margin:0; font:15px/1.45 system-ui,sans-serif; background:#0b0d12; color:#e8e4dc; }}
  .tank-wrap {{ max-width: 980px; margin: 0 auto; padding: 24px; }}
  h1,h2 {{ font-weight:600; letter-spacing:.02em; }}
  .muted {{ color:#8b93a7; }}
  .tank-svg {{ image-rendering: pixelated; background:#0b0d12; border-radius:10px;
    border:1px solid #2a3140; display:block; aspect-ratio: 32 / 9; width:100%; }}
  .acts {{ display:flex; flex-wrap:wrap; gap:8px; margin:12px 0 18px; }}
  .acts button {{ min-height:44px; padding:8px 14px; border-radius:8px;
    border:1px solid #2a3140; background:#1a2030; color:#e8e4dc; cursor:pointer; }}
  .feed-grid {{ display:grid; grid-template-columns:repeat(8,1fr); gap:4px; margin:8px 0 16px; }}
  .feed-grid button {{ min-height:36px; font-size:11px; }}
  .kpis {{ display:flex; flex-wrap:wrap; gap:12px; margin:8px 0; }}
  .kpi strong {{ font-size:1.2em; }}
  .pill {{ padding:2px 8px; border-radius:999px; font-size:12px; }}
  .pill-ok {{ background:#143d2a; color:#8ee0b0; }}
  .pill-warn {{ background:#3d3214; color:#e0c88e; }}
</style>
</head>
<body>
<main class="tank-wrap">
  <h1>Rage Matrix</h1>
  <p class="muted">Hit a mode. The panel flashes the name, then that world starts.
  Tank and war stay sealed — switching does not wipe them.</p>

  <h2>Drive the board</h2>
  <div class="acts">
    {_mode_btns()}
  </div>

  <h2>Tank</h2>
  <p class="muted">Tadpoles = prey. Chevrons = hunters. LV pup-on-kill. Caps 10/5.</p>
  <div class="acts">
    <button hx-post="/api/matrix/v1/tank/feed" hx-target="#tank-board" hx-swap="outerHTML">Feed prey</button>
    <button hx-post="/api/matrix/v1/tank/scatter" hx-target="#tank-board" hx-swap="outerHTML">Rain sugar</button>
    <button hx-post="/api/matrix/v1/tank/hunt" hx-target="#tank-board" hx-swap="outerHTML">Drop hunter</button>
    <button hx-post="/api/matrix/v1/tank/shake" hx-target="#tank-board" hx-swap="outerHTML">Shake world</button>
    <button hx-post="/api/matrix/v1/tank/seed" hx-target="#tank-board" hx-swap="outerHTML">Reseed tank</button>
  </div>
  <div class="feed-grid">
    {_feed_grid()}
  </div>

  <h2>War</h2>
  <p class="muted">Civ campaign: CAMP → MELEE → GUNS → ARTY. Match end flashes score, then NEXT.</p>
  <div class="acts">
    <button hx-post="/api/matrix/v1/war/focus" hx-target="#tank-board" hx-swap="outerHTML">Watch war</button>
    <button hx-post="/api/matrix/v1/war/civ" hx-target="#tank-board" hx-swap="outerHTML">Civ campaign</button>
    <button hx-post="/api/matrix/v1/war/brawl" hx-target="#tank-board" hx-swap="outerHTML">Fast brawl</button>
    <button hx-post="/api/matrix/v1/war/epic" hx-target="#tank-board" hx-swap="outerHTML">Epic pace</button>
    <button hx-post="/api/matrix/v1/war/age" hx-target="#tank-board" hx-swap="outerHTML">Advance age</button>
    <button hx-post="/api/matrix/v1/war/opts" hx-vals='{{"auto":"1"}}' hx-target="#tank-board" hx-swap="outerHTML">Auto next</button>
    <button hx-post="/api/matrix/v1/war/opts" hx-vals='{{"auto":"0"}}' hx-target="#tank-board" hx-swap="outerHTML">Hold for next</button>
    <button hx-post="/api/matrix/v1/war/west" hx-target="#tank-board" hx-swap="outerHTML">+ west</button>
    <button hx-post="/api/matrix/v1/war/east" hx-target="#tank-board" hx-swap="outerHTML">+ east</button>
    <button hx-post="/api/matrix/v1/war/next" hx-target="#tank-board" hx-swap="outerHTML">Next match</button>
    <button hx-post="/api/matrix/v1/war/seed" hx-target="#tank-board" hx-swap="outerHTML">New campaign</button>
  </div>

  {board}

  <section>
    <h2>Talk to the board</h2>
    <p class="muted">Serial @9600: <code>e</code> tank · <code>r</code> war · <code>f</code> feed ·
    <code>z</code> shake · <code>a</code> reel. OTA: HTTP <code>POST /update</code> field
    <code>firmware</code>.</p>
  </section>
</main>
</body>
</html>
"""
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(html, encoding="utf-8")
    return OUT


if __name__ == "__main__":
    print(compose())
