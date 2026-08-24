#!/usr/bin/env python3
"""Preview the variety reel (mirrors src/main.cpp modes)."""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from sim.engine import Display2Sim, H, W
from sim.preview import encode_mp4
from sim.render_board import render_frame

random.seed(7)

# 3x5 A-Z + digits (same idea as firmware)
AZ = {
    "A": [2, 5, 7, 5, 5], "B": [6, 5, 6, 5, 6], "C": [3, 4, 4, 4, 3],
    "D": [6, 5, 5, 5, 6], "E": [7, 4, 6, 4, 7], "F": [7, 4, 6, 4, 4],
    "G": [3, 4, 5, 5, 3], "H": [5, 5, 7, 5, 5], "I": [7, 2, 2, 2, 7],
    "J": [5, 1, 1, 5, 2],
    "K": [5, 6, 4, 6, 5],
    "Q": [2, 5, 5, 7, 3],
    "U": [5, 5, 5, 5, 7],
    "V": [5, 5, 5, 5, 2],
    "W": [5, 5, 7, 7, 5], "L": [4, 4, 4, 4, 7], "M": [5, 7, 7, 5, 5],
    "N": [5, 7, 7, 7, 5], "O": [2, 5, 5, 5, 2], "P": [6, 5, 6, 4, 4],
    "R": [6, 5, 6, 6, 5], "S": [3, 4, 2, 1, 6], "T": [7, 2, 2, 2, 2],
    "X": [5, 5, 2, 5, 5], "Y": [5, 5, 2, 2, 2], "Z": [7, 1, 2, 4, 7],
    " ": [0, 0, 0, 0, 0],
    "0": [7, 5, 5, 5, 7], "1": [2, 6, 2, 2, 7], "2": [7, 1, 7, 4, 7],
    "3": [7, 1, 7, 1, 7], "4": [5, 5, 7, 1, 1], "5": [7, 4, 7, 1, 7],
    "6": [7, 4, 7, 5, 7], "7": [7, 1, 1, 1, 1], "8": [7, 5, 7, 5, 7],
    "9": [7, 5, 7, 1, 7], ":": [0, 2, 0, 2, 0],
}


def blank():
    return [[0] * W for _ in range(H)]


def plot(px, x, y, c):
    if 0 <= x < W and 0 <= y < H and c > px[y][x]:
        px[y][x] = min(255, int(c))


def draw_text(px, x0, y0, s, c=90):
    x = x0
    for ch in s.upper():
        g = AZ.get(ch, [0, 0, 0, 0, 0])
        for r, bits in enumerate(g):
            for col in range(3):
                if bits & (4 >> col):
                    plot(px, x + col, y0 + r, c)
        x += 4


PROMO = [
    "RAGE INDUSTRIES  ",
    "ZERO-HUMAN COMPANY  ",
    "LOCAL. SOVEREIGN. ALWAYS ON.  ",
    "SPARK-NATIVE DESK GLASS  ",
    "BUILD. SHIP. RAGE.  ",
]


def mode_text(i, n):
    px = blank()
    # swap line mid-clip so the bank reads
    msg = PROMO[0 if i < n // 2 else 1]
    scroll = W - int(i * 0.8)
    w = len(msg) * 4
    while scroll < -w:
        scroll += w + W
    draw_text(px, scroll, 2, msg)
    return px


def mode_clock(i, n):
    px = blank()
    sec = i // 4
    draw_text(px, 2, 2, f"{(sec // 60) % 60:02d}:{sec % 60:02d}")
    plot(px, sec % W, 8, 50)
    return px


def mode_bars(i, n):
    px = blank()
    t = i * 0.09
    for x in range(W):
        v = 0.5 + 0.5 * math.sin(x * 0.45 + t) * math.cos(x * 0.17 - t * 0.7)
        v = max(0.0, min(1.0, v)) ** 1.3
        h = int(round(v * 8))
        for y in range(h):
            plot(px, x, H - 1 - y, 90 * (0.35 + 0.65 * y / 8.0))
    return px


def mode_spark(i, n, hist):
    hist.append(0.55 + 0.25 * math.sin(i * 0.11) + 0.12 * math.sin(i * 0.03))
    hist[:] = hist[-W:]
    px = blank()
    for x in range(1, len(hist)):
        y0 = int(round(max(0, min(1, hist[x - 1])) * 8))
        y1 = int(round(max(0, min(1, hist[x])) * 8))
        step = 1 if y1 >= y0 else -1
        y = y0
        while True:
            plot(px, x, 8 - y, 90)
            if y == y1:
                break
            y += step
    return px


def mode_rain(i, n, drops):
    px = blank()
    for x, d in drops.items():
        y = (d["y"] + i * d["v"]) % (H + 5)
        plot(px, x, int(y), 90)
        plot(px, x, int(y) - 1, 30)
    return px


def mode_bounce(i, n):
    px = blank()
    x = abs((i * 0.35) % (2 * (W - 1)) - (W - 1))
    y = abs((i * 0.22) % (2 * (H - 1)) - (H - 1))
    xi, yi = int(round(x)), int(round(y))
    plot(px, xi, yi, 90)
    plot(px, xi - 1, yi, 22)
    plot(px, xi + 1, yi, 22)
    plot(px, xi, yi - 1, 22)
    plot(px, xi, yi + 1, 22)
    return px


def mode_life(i, n, grid):
    if i % 4 == 0:
        nxt = [[0] * W for _ in range(H)]
        for y in range(H):
            for x in range(W):
                c = 0
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx or dy:
                            c += grid[(y + dy) % H][(x + dx) % W]
                nxt[y][x] = 1 if (c == 3 or (grid[y][x] and c == 2)) else 0
        for y in range(H):
            grid[y] = nxt[y]
    px = blank()
    for y in range(H):
        for x in range(W):
            if grid[y][x]:
                plot(px, x, y, 90)
    return px


def mode_pulse(i, n):
    px = blank()
    t = (i % 40) / 40.0
    beat = math.exp(-t * 6.0) + 0.35 * math.exp(-abs(t - 0.22) * 14.0)
    r = 1.0 + beat * 5.0
    for y in range(H):
        for x in range(W):
            d = math.sqrt((x - 16) ** 2 + (y - 4) ** 2 * 2.4)
            v = max(0.0, 1.0 - abs(d - r) * 0.9) ** 1.6
            if v > 0.15:
                plot(px, x, y, v * 90)
    return px


def mode_text_line(msg):
    def fn(i, n):
        px = blank()
        scroll = W - int(i * 1.0)
        w = len(msg) * 4
        while scroll < -w:
            scroll += w + W
        draw_text(px, scroll, 2, msg)
        return px

    return fn


def main() -> int:
    fps = 16
    frames_dir = ROOT / "preview" / "_frames"
    if frames_dir.exists():
        for p in frames_dir.glob("*.png"):
            p.unlink()
    frames_dir.mkdir(parents=True, exist_ok=True)

    sim = Display2Sim()
    hist = [0.5] * W
    drops = {x: {"y": random.randint(0, H), "v": random.choice([1, 2])} for x in range(0, W, 2)}
    grid = [[1 if random.random() < 0.28 else 0 for _ in range(W)] for _ in range(H)]

    visuals = [
        ("wave", None, "cyan", 3.0),
        ("sine", None, "amber", 3.0),
        ("clock", mode_clock, "white", 2.5),
        ("bars", mode_bars, "violet", 3.0),
        ("spark", None, "rage", 3.0),
        ("rain", None, "cyan", 2.5),
        ("bounce", mode_bounce, "amber", 2.5),
        ("life", None, "violet", 3.0),
        ("pulse", mode_pulse, "rage", 2.5),
    ]
    timeline = []
    for i, vis in enumerate(visuals):
        line = PROMO[i % len(PROMO)].strip()
        timeline.append((f"text · {line}", mode_text_line(PROMO[i % len(PROMO)]), "white", 2.4))
        timeline.append(vis)

    n = 0
    for name, fn, color, secs in timeline:
        sim.seed()
        total = max(8, int(secs * fps))
        for i in range(total):
            if name.startswith("wave") or name == "wave":
                px = sim.tick("wave", dt_ms=int(1000 / fps))
            elif name == "sine":
                px = sim.tick("sine", dt_ms=int(1000 / fps))
            elif name == "spark":
                px = mode_spark(i, total, hist)
            elif name == "rain":
                px = mode_rain(i, total, drops)
            elif name == "life":
                px = mode_life(i, total, grid)
            else:
                px = fn(i, total)
            img = render_frame(
                px,
                caption=f"RAGE INDUSTRIES  ·  {name}",
                color=color,
            )
            img.save(frames_dir / f"f{n:05d}.png")
            n += 1

    out = ROOT / "preview" / "variety.mp4"
    encode_mp4(frames_dir, out, fps)
    print(out, "frames", n)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
