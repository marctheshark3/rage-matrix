#!/usr/bin/env python3
"""Army twin — 48×18 bounded field, 32×9 camera. Mirrors firmware war.cpp."""
from __future__ import annotations

import math
import random
from dataclasses import dataclass, field

VW, VH, WW, WH = 32, 9, 48, 18
MAX_U, MAX_S = 16, 14
TEAM_W, TEAM_E = 0, 1


def _clamp(x, a, b):
    return a if x < a else b if x > b else x


@dataclass
class Unit:
    x: float
    y: float
    a: float
    hp: float
    g: list
    team: int
    gen: int = 0
    cd: int = 0
    alive: bool = True
    role: int = 0
    kills: int = 0
    dmg: int = 0


@dataclass
class Shot:
    x: float
    y: float
    dx: float
    dy: float
    team: int
    dmg: int
    splash: int
    on: bool = True
    life: int = 28
    arty: int = 0


@dataclass
class War:
    bright: int = 90
    cam_x: float = 8.0
    cam_y: float = 4.5
    match: int = 0
    tick: int = 0
    match_tick: int = 0
    max_gen: int = 0
    kill_w: int = 0
    kill_e: int = 0
    units: list = field(default_factory=list)
    shots: list = field(default_factory=list)
    wall: list = field(default_factory=list)
    elite: list = field(default_factory=lambda: [[], []])
    boom: list = field(default_factory=lambda: [[0] * VW for _ in range(VH)])

    def seed(self) -> None:
        self.elite = [[], []]
        self.max_gen = 0
        self.match = 0
        self.kill_w = self.kill_e = 0
        self.tick = 0
        self._build_map()
        self._deploy(False)

    def rematch(self) -> None:
        self._harvest()
        self._deploy(True)

    def _mutate(self, g):
        out = list(g)
        for _ in range(2):
            i = random.randrange(8)
            out[i] = max(0, min(255, out[i] + random.randint(-25, 25)))
        return out

    def _build_map(self) -> None:
        self.wall = [[0] * WW for _ in range(WH)]
        for x in range(WW):
            self.wall[0][x] = self.wall[WH - 1][x] = 1
        for y in range(WH):
            self.wall[y][0] = self.wall[y][WW - 1] = 1
        for _ in range(3):
            x, y, ln, vert = 16 + random.randrange(16), 2 + random.randrange(WH - 5), 3 + random.randrange(5), random.randrange(2)
            for i in range(ln):
                xx, yy = (x, y + i) if vert else (x + i, y)
                if 2 < xx < WW - 3 and 1 < yy < WH - 2:
                    self.wall[yy][xx] = 1
        for _ in range(2):
            x, y = 18 + random.randrange(12), 4 + random.randrange(8)
            for dy in range(2):
                for dx in range(3):
                    if y + dy < WH - 2 and x + dx < WW - 3:
                        self.wall[y + dy][x + dx] = 2

    def _blocked(self, x, y) -> bool:
        if x < 0 or y < 0 or x >= WW or y >= WH:
            return True
        return self.wall[y][x] != 0

    def _los(self, x0, y0, x1, y1, arty: bool) -> bool:
        dx, dy = x1 - x0, y1 - y0
        dist = math.hypot(dx, dy)
        if dist < 0.2:
            return True
        steps = int(dist * 2) + 1
        skipped = 0
        for i in range(1, steps):
            t = i / steps
            gx, gy = int(math.floor(x0 + dx * t)), int(math.floor(y0 + dy * t))
            if not self._blocked(gx, gy):
                continue
            if arty and skipped == 0:
                skipped = 1
                continue
            return False
        return True

    def _spawn(self, x, y, team, role, src, gen) -> None:
        if sum(1 for u in self.units if u.alive) >= MAX_U:
            return
        g = self._mutate(src) if src else [random.randint(40, 220) for _ in range(8)]
        if g[6] > 200:
            role = 1
        self.units.append(
            Unit(_clamp(x, 1, WW - 2), _clamp(y, 1, WH - 2), 0.0 if team == TEAM_W else math.pi, 1.35 if role else 1.0, g, team, gen, random.randint(0, 12), True, role)
        )

    def _deploy(self, evolve: bool) -> None:
        self.units = []
        self.shots = []
        self.boom = [[0] * VW for _ in range(VH)]
        self.match_tick = 0
        self.match += 1
        for t in (TEAM_W, TEAM_E):
            for i in range(6):
                arty = i == 0
                x = (2.5 + random.random() * 6) if t == TEAM_W else (WW - 3.5 - random.random() * 6)
                y = 2 + random.random() * (WH - 4)
                src, gen = None, 0
                if evolve and self.elite[t]:
                    src = random.choice(self.elite[t])
                    gen = self.max_gen
                self._spawn(x, y, t, 1 if arty else 0, src, gen)
        self.cam_x, self.cam_y = 8.0, 4.5

    def _harvest(self) -> None:
        scored = []
        for u in self.units:
            if not u.alive and u.kills == 0 and u.dmg == 0:
                continue
            scored.append((u.kills * 80 + u.dmg + int(u.hp * 20), u))
        for t in (TEAM_W, TEAM_E):
            top = sorted([(s, i) for i, (s, u) in enumerate(scored) if u.team == t], reverse=True)[:4]
            self.elite[t] = [list(scored[i][1].g) for _, i in top]
        if self.max_gen < 250:
            self.max_gen += 1

    def n(self, team):
        return sum(1 for u in self.units if u.alive and u.team == team)

    def drop_wall(self, vx=16, vy=4) -> None:
        gx = int(_clamp(int(self.cam_x) + vx, 2, WW - 3))
        gy = int(_clamp(int(self.cam_y) + vy, 2, WH - 3))
        for dy in (-1, 0, 1):
            for dx in (0, 1, 2):
                self.wall[gy + dy][gx + dx] = 2 if dx == 1 and dy == 0 else 1

    def reinforce(self, team, arty=False) -> None:
        src = self.elite[team][0] if self.elite[team] else None
        x = 3.5 if team == TEAM_W else WW - 4.0
        self._spawn(x, 3 + random.random() * (WH - 6), team, 1 if arty else 0, src, self.max_gen)

    def step(self) -> None:
        self.tick += 1
        self.match_tick += 1
        for y in range(VH):
            for x in range(VW):
                self.boom[y][x] = max(0, self.boom[y][x] - 8)
        for u in self.units:
            if not u.alive:
                continue
            if u.cd:
                u.cd -= 1
            speed = (0.06 if u.role else 0.12) + (u.g[0] / 255.0) * (0.10 if u.role else 0.22)
            turn = 0.16 + (u.g[1] / 255.0) * 0.45
            rng = (10.0 if u.role else 5.5) + (u.g[2] / 255.0) * (12.0 if u.role else 7.0)
            acc = 0.55 + (u.g[4] / 255.0) * 0.40
            foes = [o for o in self.units if o.alive and o.team != u.team]
            if not foes:
                continue
            e = min(foes, key=lambda o: (o.x - u.x) ** 2 + (o.y - u.y) ** 2)
            edx, edy = e.x - u.x, e.y - u.y
            ed2 = edx * edx + edy * edy
            can = self._los(u.x, u.y, e.x, e.y, bool(u.role))
            hold = 11.0 if u.team == TEAM_W else WW - 12.0
            if u.role:
                want = math.atan2(e.y - u.y, hold - u.x)
            elif can and ed2 < rng * rng * 0.55:
                want = math.atan2(edy, edx)
                speed *= 0.6
                u.a += 1.2
            else:
                want = math.atan2(edy, edx)
            da = (want - u.a + math.pi) % math.tau - math.pi
            u.a += max(-turn, min(turn, da))
            nx, ny = u.x + math.cos(u.a) * speed, u.y + math.sin(u.a) * speed
            if self._blocked(int(math.floor(nx)), int(math.floor(ny))):
                u.a += (random.random() - 0.5) * 1.4
            else:
                u.x, u.y = _clamp(nx, 1.1, WW - 2.1), _clamp(ny, 1.1, WH - 2.1)
            if u.cd == 0 and can and ed2 < rng * rng and len(self.shots) < MAX_S:
                j = (1.0 - acc) * 2.2
                d = math.hypot(e.x - u.x, e.y - u.y) or 1
                spd = 0.42 if u.role else 0.55
                self.shots.append(
                    Shot(u.x, u.y, (e.x + (random.random() - 0.5) * j - u.x) / d * spd, (e.y + (random.random() - 0.5) * j - u.y) / d * spd, u.team, 22 if u.role else 14, 2 if u.role else 0, True, 28, u.role)
                )
                u.cd = (18 + (255 - u.g[3]) // 14) if u.role else (8 + (255 - u.g[3]) // 20)
        live = []
        for s in self.shots:
            if not s.on:
                continue
            s.x += s.dx
            s.y += s.dy
            s.life -= 1
            gx, gy = int(math.floor(s.x)), int(math.floor(s.y))
            hit = self._blocked(gx, gy)
            if (hit and not s.arty) or s.life <= 0 or not (0 <= s.x < WW and 0 <= s.y < WH):
                continue
            if hit and s.arty and 0 <= gy < WH and 0 <= gx < WW and self.wall[gy][gx] == 1:
                live.append(s)
                continue
            if hit and s.arty and 0 <= gy < WH and 0 <= gx < WW and self.wall[gy][gx] == 2:
                continue
            struck = False
            for o in self.units:
                if not o.alive or o.team == s.team:
                    continue
                rad = 1.7 if s.splash else 0.72
                if (o.x - s.x) ** 2 + (o.y - s.y) ** 2 > rad * rad:
                    continue
                armor = 0.55 + (o.g[5] / 255.0) * 0.40
                take = (s.dmg / 100.0) * (1.4 - armor)
                o.hp -= take
                vx, vy = int(round(s.x - self.cam_x)), int(round(s.y - self.cam_y))
                if 0 <= vx < VW and 0 <= vy < VH:
                    self.boom[vy][vx] = 90
                if o.hp <= 0:
                    o.alive = False
                    if s.team == TEAM_W:
                        self.kill_w += 1
                    else:
                        self.kill_e += 1
                struck = True
                break
            if not struck:
                live.append(s)
        self.shots = live
        nw, ne = self.n(TEAM_W), self.n(TEAM_E)
        def com(team, fb):
            pack = [u for u in self.units if u.alive and u.team == team]
            if not pack:
                return fb
            return sum(u.x for u in pack) / len(pack), sum(u.y for u in pack) / len(pack)
        wx, wy = com(TEAM_W, (8, 9))
        ex, ey = com(TEAM_E, (40, 9))
        tx = _clamp((wx + ex) * 0.5 - VW * 0.5, 0, WW - VW)
        ty = _clamp((wy + ey) * 0.5 - VH * 0.5, 0, WH - VH)
        self.cam_x += (tx - self.cam_x) * 0.06
        self.cam_y += (ty - self.cam_y) * 0.06
        if (nw == 0 or ne == 0 or self.match_tick > 2200) and self.match_tick > 80:
            self.rematch()

    def render(self):
        fb = [[0] * VW for _ in range(VH)]
        b = self.bright
        for y in range(VH):
            for x in range(VW):
                gx, gy = int(math.floor(self.cam_x)) + x, int(math.floor(self.cam_y)) + y
                if 0 <= gx < WW and 0 <= gy < WH and self.wall[gy][gx]:
                    fb[y][x] = max(fb[y][x], int(b * (0.48 if self.wall[gy][gx] == 2 else 0.28)))
        for s in self.shots:
            if not s.on:
                continue
            x, y = int(round(s.x - self.cam_x)), int(round(s.y - self.cam_y))
            if 0 <= x < VW and 0 <= y < VH:
                fb[y][x] = b
        for u in self.units:
            if not u.alive:
                continue
            x, y = int(round(u.x - self.cam_x)), int(round(u.y - self.cam_y))
            core = int(max(12, min(b, b * (0.40 + 0.55 * max(0, min(1.2, u.hp))))))
            def p(xx, yy, c):
                if 0 <= xx < VW and 0 <= yy < VH:
                    fb[yy][xx] = max(fb[yy][xx], c)
            if u.role:
                p(x, y, core)
                p(x + 1, y, core // 2)
                p(x - 1, y, core // 2)
                p(x, y + 1, core // 2)
                p(x, y - 1, core // 2)
            elif u.team == TEAM_W:
                p(x, y, core)
                p(x + 1, y, core // 3)
            else:
                p(x, y, core)
                p(x - 1, y, core // 3)
        for y in range(VH):
            for x in range(VW):
                fb[y][x] = max(fb[y][x], min(b, self.boom[y][x]))
        return fb

    def px_hex(self) -> str:
        return "".join(f"{v:02x}" for row in self.render() for v in row)

    def state(self) -> dict:
        return {
            "ok": True,
            "sim": "war",
            "w": 32,
            "h": 9,
            "west": self.n(TEAM_W),
            "east": self.n(TEAM_E),
            "match": self.match,
            "gen": self.max_gen,
            "kw": self.kill_w,
            "ke": self.kill_e,
            "tick": self.match_tick,
            "cam": [round(self.cam_x, 1), round(self.cam_y, 1)],
            "imu": False,
            "ldr": False,
            "source": "twin",
        }
