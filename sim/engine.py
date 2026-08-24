"""Firmware-faithful 32x9 field. Keep in lockstep with src/main.cpp."""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Literal

W, H = 32, 9
Mode = Literal["wave", "plasma", "sine"]


@dataclass
class Display2Sim:
    bright: int = 90
    c2: float = 0.20
    damp: float = 0.994
    k_off: float = 0.38
    frame: int = 0
    drop_every_ms: int = 1400
    last_drop_ms: int = 0
    rng: int = 0xC0FFEE
    u: list = field(default_factory=lambda: [[0.0] * W for _ in range(H)])
    v: list = field(default_factory=lambda: [[0.0] * W for _ in range(H)])

    def _rand(self, lo: int, hi: int) -> int:
        # xorshift-ish, deterministic preview
        self.rng ^= (self.rng << 13) & 0xFFFFFFFF
        self.rng ^= (self.rng >> 17) & 0xFFFFFFFF
        self.rng ^= (self.rng << 5) & 0xFFFFFFFF
        span = hi - lo
        if span <= 0:
            return lo
        return lo + (self.rng % span)

    def clear(self) -> None:
        for y in range(H):
            for x in range(W):
                self.u[y][x] = 0.0
                self.v[y][x] = 0.0

    def drop_at(self, cx: int, cy: int, amp: float = 1.0) -> None:
        for y in range(H):
            for x in range(W):
                dx = float(x - cx)
                dy = float(y - cy) * 1.6
                r2 = dx * dx + dy * dy
                if r2 < 20.0:
                    bump = amp * math.exp(-r2 * 0.35)
                    self.u[y][x] += bump
                    self.v[y][x] += bump * 0.85

    def seed(self) -> None:
        self.clear()
        self.drop_at(8, 4, 1.0)
        self.drop_at(22, 3, 0.85)
        self.drop_at(15, 6, 0.7)
        self.last_drop_ms = 0
        self.frame = 0

    def wave_step(self) -> None:
        nxt = [[0.0] * W for _ in range(H)]
        for y in range(H):
            for x in range(W):
                left = self.u[y][x - 1 if x > 0 else x]
                right = self.u[y][x + 1 if x < W - 1 else x]
                up = self.u[y - 1 if y > 0 else y][x]
                down = self.u[y + 1 if y < H - 1 else y][x]
                lap = (left + right + up + down) - 4.0 * self.u[y][x]
                n = (2.0 * self.u[y][x] - self.v[y][x]) + self.c2 * lap
                n *= self.damp
                nxt[y][x] = max(-1.5, min(1.5, n))
        self.v = self.u
        self.u = nxt

    def sparse_px(self, n: float) -> int:
        if n < self.k_off:
            return 0
        t = (n - self.k_off) / (1.0 - self.k_off)
        t = t * t
        c = int(t * self.bright)
        return 0 if c < 4 else min(255, c)

    def pixels_wave(self) -> list[list[int]]:
        px = [[0] * W for _ in range(H)]
        t = self.frame * 0.07
        for x in range(W):
            e = sum(self.u[y][x] for y in range(H)) / float(H)
            yf = 4.0 + e * 3.6 + 1.6 * math.sin(x * 0.38 - t)
            y0 = math.floor(yf)
            frac = yf - y0
            if 0 <= y0 < H:
                px[y0][x] = max(px[y0][x], int((1.0 - frac) * self.bright))
            if 0 <= y0 + 1 < H:
                px[y0 + 1][x] = max(px[y0 + 1][x], int(frac * self.bright * 0.55))
        for y in range(H):
            for x in range(W):
                c = self.sparse_px(abs(self.u[y][x]))
                if c > px[y][x]:
                    px[y][x] = c
        return px

    def pixels_plasma(self) -> list[list[int]]:
        px = [[0] * W for _ in range(H)]
        t = self.frame * 0.045
        for y in range(H):
            for x in range(W):
                v1 = math.sin(x * 0.35 + t)
                v2 = math.sin(y * 0.55 + t * 1.3)
                v3 = math.sin((x + y) * 0.25 + t * 0.7)
                dx = float(x - 16)
                dy = float(y - 4) * 1.5
                v4 = math.sin(math.sqrt(dx * dx + dy * dy) * 0.4 - t)
                n = (v1 + v2 + v3 + v4 + 4.0) * 0.125
                n = n ** 2.2
                px[y][x] = self.sparse_px(n)
        return px

    def pixels_sine(self) -> list[list[int]]:
        px = [[0] * W for _ in range(H)]
        t = self.frame * 0.08
        for x in range(W):
            yf = math.sin(x * 0.4 - t) * 3.1 + 4.0
            y0 = math.floor(yf)
            frac = yf - y0
            if 0 <= y0 < H:
                px[y0][x] = max(px[y0][x], int((1.0 - frac) * self.bright))
            if 0 <= y0 + 1 < H:
                px[y0 + 1][x] = max(px[y0 + 1][x], int(frac * self.bright * 0.4))
            y2 = math.sin(x * 0.22 + t * 0.6 + 1.2) * 2.2 + 4.0
            yi = int(round(y2))
            if 0 <= yi < H:
                px[yi][x] = max(px[yi][x], self.bright // 3)
        return px

    def pixels(self, mode: Mode) -> list[list[int]]:
        if mode == "wave":
            return self.pixels_wave()
        if mode == "plasma":
            return self.pixels_plasma()
        return self.pixels_sine()

    def tick(self, mode: Mode, dt_ms: int = 33) -> list[list[int]]:
        now = self.frame * dt_ms
        if mode == "wave":
            if now - self.last_drop_ms > self.drop_every_ms:
                self.last_drop_ms = now
                self.drop_every_ms = 900 + self._rand(0, 1600)
                self.drop_at(
                    self._rand(1, W - 1),
                    self._rand(0, H),
                    0.7 + self._rand(0, 50) / 100.0,
                )
            self.wave_step()
        out = self.pixels(mode)
        self.frame += 1
        return out
