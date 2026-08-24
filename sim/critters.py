#!/usr/bin/env python3
"""Evolutionary petri twin — 64×18 world, 32×9 camera. Mirrors firmware critters.cpp."""
from __future__ import annotations

import math
import random
from dataclasses import dataclass, field

VW, VH = 32, 9
WW, WH = 64.0, 18.0
MAX_C, MAX_F = 14, 20
KIND_PREY, KIND_PRED = 0, 1


def _wrap(v: float, m: float) -> float:
    return v % m


def _wrapd(d: float, m: float) -> float:
    if d > m * 0.5:
        d -= m
    if d < -m * 0.5:
        d += m
    return d


@dataclass
class Critter:
    x: float
    y: float
    a: float
    e: float
    g: list
    gen: int = 0
    kind: int = KIND_PREY
    alive: bool = True


@dataclass
class Food:
    x: float
    y: float
    a: float
    on: bool = True


@dataclass
class Tank:
    bright: int = 90
    follow: bool = True
    cam_x: float = 16.0
    cam_y: float = 4.5
    shake: float = 0.0
    births: int = 0
    tick: int = 0
    critters: list = field(default_factory=list)
    foods: list = field(default_factory=list)
    trail: list = field(default_factory=lambda: [[0] * VW for _ in range(VH)])

    def seed(self) -> None:
        self.critters = []
        self.foods = []
        self.trail = [[0] * VW for _ in range(VH)]
        self.births = 0
        self.cam_x, self.cam_y = 16.0, 4.5
        self.shake = 0.0
        for _ in range(8):
            self._spawn(6 + random.random() * 52, 2 + random.random() * 14, None, 0, 0.65 + random.random() * 0.35, KIND_PREY)
        for _ in range(3):
            self._spawn(random.random() * WW, random.random() * WH, None, 0, 0.85 + random.random() * 0.25, KIND_PRED)
        for _ in range(10):
            self._drop(random.random() * WW, random.random() * WH, 0.6 + random.random() * 0.4)

    def _mutate(self, g: list) -> list:
        out = list(g)
        for _ in range(2):
            i = random.randrange(8)
            out[i] = max(0, min(255, out[i] + random.randint(-25, 25)))
        return out

    def _spawn(self, x, y, src, gen, e, kind=KIND_PREY) -> bool:
        if sum(1 for c in self.critters if c.alive) >= MAX_C:
            return False
        g = self._mutate(src) if src else [random.randint(40, 220) for _ in range(8)]
        self.critters.append(
            Critter(_wrap(x, WW), _wrap(y, WH), random.random() * math.tau, e, g, gen, kind, True)
        )
        self.births += 1
        return True

    def _drop(self, x, y, a) -> None:
        dead = next((f for f in self.foods if not f.on), None)
        if dead is None and len(self.foods) < MAX_F:
            self.foods.append(Food(_wrap(x, WW), _wrap(y, WH), max(0.25, min(1.2, a)), True))
            return
        if not self.foods:
            return
        slot = dead or random.choice(self.foods)
        slot.on, slot.x, slot.y, slot.a = True, _wrap(x, WW), _wrap(y, WH), max(0.25, min(1.2, a))

    def _nearest_kind(self, c: Critter, kind: int, rng: float):
        best, bd, dx, dy = None, rng * rng, 0.0, 0.0
        for o in self.critters:
            if o is c or not o.alive or o.kind != kind:
                continue
            ddx, ddy = _wrapd(o.x - c.x, WW), _wrapd(o.y - c.y, WH)
            d2 = ddx * ddx + ddy * ddy
            if d2 < bd:
                best, bd, dx, dy = o, d2, ddx, ddy
        return best, bd, dx, dy

    def _nearest_food(self, c: Critter):
        rng = 4.0 + (c.g[2] / 255.0) * 18.0
        best, bd, fdx, fdy = None, rng * rng, 0.0, 0.0
        for f in self.foods:
            if not f.on:
                continue
            dx, dy = _wrapd(f.x - c.x, WW), _wrapd(f.y - c.y, WH)
            d2 = dx * dx + dy * dy
            if d2 < bd:
                best, bd, fdx, fdy = f, d2, dx, dy
        return best, bd, fdx, fdy

    def _steer(self, c, dx, dy, turn, k=1.0):
        want = math.atan2(dy, dx)
        da = _wrapd(want - c.a, math.tau)
        c.a += max(-turn, min(turn, da)) * k

    def step(self) -> None:
        self.tick += 1
        if self.shake > 0:
            self.shake *= 0.86
        if self.tick % 12 == 0:
            self._drop(random.random() * WW, random.random() * WH, 0.45 + 0.30 * random.random())

        n = 0
        for c in self.critters:
            if not c.alive:
                continue
            pred = c.kind == KIND_PRED
            speed = (0.14 if pred else 0.22) + (c.g[0] / 255.0) * (0.26 if pred else 0.38)
            turn = 0.14 + (c.g[1] / 255.0) * 0.50
            metab = (0.00055 + (c.g[7] / 255.0) * 0.00055) if pred else (0.00028 + (c.g[7] / 255.0) * 0.00032)
            rng = 6.0 + (c.g[2] / 255.0) * (18.0 if pred else 12.0)
            conv = 0.70 + (c.g[3] / 255.0) * 0.35
            prey_split = 0.48 + (c.g[3] / 255.0) * 0.18
            if self.shake > 0.05:
                c.a += (random.random() - 0.5) * self.shake * 2.4
                speed += self.shake * 0.35
            if pred:
                prey, pd2, pdx, pdy = self._nearest_kind(c, KIND_PREY, rng)
                if prey:
                    self._steer(c, pdx, pdy, turn)
                    if pd2 < 1.45:
                        c.e = min(2.2, c.e + 0.45 + prey.e * conv)
                        self._drop(prey.x, prey.y, 0.18)
                        prey.alive = False
                        if self.pred_n() < 5:
                            pup = 0.55 + 0.15 * (c.g[3] / 255.0)
                            if self._spawn(c.x, c.y, c.g, c.gen + 1, pup, KIND_PRED):
                                c.e = max(0.35, c.e - 0.22)
                else:
                    c.a += (random.random() - 0.5) * turn * 0.8
            else:
                c.a += (random.random() - 0.5) * (0.22 + (c.g[1] / 255.0) * 0.28)
                hunter, hd2, hdx, hdy = self._nearest_kind(c, KIND_PRED, rng)
                food, fd2, fdx, fdy = self._nearest_food(c)
                school, sd2, sdx, sdy = self._nearest_kind(c, KIND_PREY, 9.0)
                if hunter and hd2 < 49:
                    self._steer(c, -hdx, -hdy, turn, 1.25)
                    speed += 0.20
                else:
                    if food and fd2 > 1.8:
                        self._steer(c, fdx, fdy, turn, 0.45)
                    if school and 2.8 < sd2 < 80:
                        self._steer(c, sdx, sdy, turn, 0.30)
                if food and fd2 < 0.95:
                    c.e = min(1.8, c.e + food.a * 0.62)
                    food.a -= 0.40
                    if food.a < 0.12:
                        food.on = False
                    c.a += 2.4 + (random.random() - 0.5) * 1.4
            c.x = _wrap(c.x + math.cos(c.a) * speed, WW)
            c.y = _wrap(c.y + math.sin(c.a) * speed, WH)
            c.e -= metab
            if not pred and c.e > prey_split and self.prey_n() < 10:
                if self._spawn(c.x, c.y, c.g, c.gen + 1, 0.38, KIND_PREY):
                    c.e -= 0.28
            if c.e < 0.04:
                if not pred:
                    self._drop(c.x, c.y, 0.28)
                c.alive = False
                continue
            n += 1
        prey_n, pred_n = self.prey_n(), self.pred_n()
        if pred_n == 0 and prey_n >= 6 and self.tick % 180 == 0:
            self._spawn(random.random() * WW, random.random() * WH, None, 0, 0.9, KIND_PRED)
        if prey_n == 0 and pred_n > 0 and self.tick % 120 == 0:
            self._spawn(random.random() * WW, random.random() * WH, None, 0, 0.7, KIND_PREY)
        if n == 0:
            self.seed()
        if self.follow:
            pack = [c for c in self.critters if c.alive and (pred_n == 0 or c.kind == KIND_PRED)]
            if pack:
                tx = sum(_wrapd(c.x - self.cam_x, WW) + self.cam_x for c in pack) / len(pack)
                ty = sum(_wrapd(c.y - self.cam_y, WH) + self.cam_y for c in pack) / len(pack)
                self.cam_x = _wrap(self.cam_x + _wrapd(tx - VW * 0.5 - self.cam_x, WW) * 0.04, WW)
                self.cam_y = _wrap(self.cam_y + _wrapd(ty - VH * 0.5 - self.cam_y, WH) * 0.04, WH)
        for y in range(VH):
            for x in range(VW):
                self.trail[y][x] = max(0, self.trail[y][x] - 4)

    def _plot(self, fb, x, y, c):
        if 0 <= x < VW and 0 <= y < VH:
            fb[y][x] = max(fb[y][x], c)

    def render(self) -> list[list[int]]:
        fb = [[0] * VW for _ in range(VH)]
        b = self.bright
        for f in self.foods:
            if not f.on:
                continue
            x = int(math.floor(_wrapd(f.x - self.cam_x, WW)))
            y = int(math.floor(_wrapd(f.y - self.cam_y, WH)))
            self._plot(fb, x, y, int(min(b * 0.42, f.a * b * 0.40)))
        fx = [1, 1, 0, -1, -1, -1, 0, 1]
        fy = [0, -1, -1, -1, 0, 1, 1, 1]
        for c in self.critters:
            if not c.alive:
                continue
            x = int(round(_wrapd(c.x - self.cam_x, WW)))
            y = int(round(_wrapd(c.y - self.cam_y, WH)))
            core = int(max(10, min(b, b * (0.45 + 0.55 * max(0, min(1, c.e))))))
            if 0 <= x < VW and 0 <= y < VH:
                dep = 18 if c.kind == KIND_PRED else int(6 + (c.g[5] / 255.0) * 28)
                self.trail[y][x] = max(self.trail[y][x], dep)
            if c.kind == KIND_PRED:
                d = int(round(c.a / 0.785398)) & 7
                dx, dy = fx[d], fy[d]
                self._plot(fb, x, y, core)
                self._plot(fb, x + dx, y + dy, core)
                self._plot(fb, x - dy, y + dx, core // 3)
                self._plot(fb, x + dy, y - dx, core // 3)
            else:
                d = int(round(c.a / 0.785398)) & 7
                self._plot(fb, x, y, core)
                self._plot(fb, x - fx[d], y - fy[d], core // 3)
        for y in range(VH):
            for x in range(VW):
                fb[y][x] = max(fb[y][x], self.trail[y][x])
        if self.shake > 0.2:
            rim = int(min(b, self.shake * b * 0.5))
            for x in range(VW):
                fb[0][x] = max(fb[0][x], rim)
                fb[VH - 1][x] = max(fb[VH - 1][x], rim)
        return fb

    def feed(self, vx: int | None = None, vy: int | None = None) -> None:
        x = self.cam_x + (vx if vx is not None else random.randint(4, 27)) + 0.5
        y = self.cam_y + (vy if vy is not None else random.randint(1, 7)) + 0.5
        self._drop(x, y, 1.05)
        self._drop(x + 0.8, y - 0.4, 0.7)

    def scatter(self) -> None:
        for _ in range(6):
            self._drop(random.random() * WW, random.random() * WH, 0.8)

    def do_shake(self, amp: float = 1.2) -> None:
        self.shake = max(0.3, min(2.4, amp))
        for c in self.critters:
            if c.alive:
                c.a += (random.random() - 0.5) * 3.2 * amp
                c.e *= 0.96
        for f in self.foods:
            if f.on:
                f.x = _wrap(f.x + (random.random() - 0.5) * 4 * amp, WW)
                f.y = _wrap(f.y + (random.random() - 0.5) * 2.2 * amp, WH)

    def drop_hunter(self) -> None:
        self._spawn(self.cam_x + 16, self.cam_y + 4, None, 0, 1.0, KIND_PRED)

    def alive(self) -> int:
        return sum(1 for c in self.critters if c.alive)

    def prey_n(self) -> int:
        return sum(1 for c in self.critters if c.alive and c.kind == KIND_PREY)

    def pred_n(self) -> int:
        return sum(1 for c in self.critters if c.alive and c.kind == KIND_PRED)

    def max_gen(self) -> int:
        return max((c.gen for c in self.critters if c.alive), default=0)

    def food_n(self) -> int:
        return sum(1 for f in self.foods if f.on)

    def px_hex(self) -> str:
        fb = self.render()
        return "".join(f"{v:02x}" for row in fb for v in row)

    def state(self) -> dict:
        return {
            "ok": True,
            "w": 32,
            "h": 9,
            "alive": self.alive(),
            "prey": self.prey_n(),
            "pred": self.pred_n(),
            "births": self.births,
            "gen": self.max_gen(),
            "food": self.food_n(),
            "adc": None,
            "vbatt": None,
            "follow": self.follow,
            "shake": round(self.shake, 2),
            "cam": [round(self.cam_x, 1), round(self.cam_y, 1)],
            "imu": False,
            "ldr": False,
            "source": "twin",
        }
