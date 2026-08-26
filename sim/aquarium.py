#!/usr/bin/env python3
"""Sealed 3D-ish aquarium twin: 56x18 world through a 32x9 camera."""
from __future__ import annotations

import random
from dataclasses import dataclass, field

VIEW_W, VIEW_H = 32, 9
WORLD_W, WORLD_H = 56.0, 18.0


@dataclass
class Fish:
    x: float
    y: float
    z: float
    dz: float
    direction: int
    speed: float
    phase: float


@dataclass
class Bubble:
    x: float
    y: float
    z: float
    speed: float


@dataclass
class Aquarium:
    seed: int = 0xA91
    fish_count: int = 12
    bright: int = 90
    cam_x: float = 20.0
    cam_y: float = 4.5
    tick: int = 0
    fish: list[Fish] = field(default_factory=list)
    plants: list[tuple[float, float, float]] = field(default_factory=list)
    rocks: list[tuple[float, float, float]] = field(default_factory=list)
    bubbles: list[Bubble] = field(default_factory=list)
    _rng: random.Random = field(init=False, repr=False)

    def __post_init__(self) -> None:
        self._rng = random.Random(self.seed)
        self.seed_world()

    def seed_world(self) -> None:
        """Create this world's persistent population; mode changes never call this."""
        self.tick = 0
        self.cam_x, self.cam_y = 20.0, 4.5
        self.fish = []
        self.plants = []
        self.rocks = []
        self.bubbles = []
        for i in range(self.fish_count):
            self.add_fish(
                3.0 + self._rng.random() * (WORLD_W - 6.0),
                2.0 + self._rng.random() * (WORLD_H - 6.0),
                0.08 + self._rng.random() * 0.88,
                1 if i % 2 == 0 else -1,
            )
        for _ in range(6):
            self.plants.append((3.0 + self._rng.random() * (WORLD_W - 6.0), WORLD_H - 1.0, 0.1 + self._rng.random() * 0.55))
        for _ in range(5):
            self.rocks.append((2.0 + self._rng.random() * (WORLD_W - 4.0), WORLD_H - 1.0, 0.1 + self._rng.random() * 0.75))
        for _ in range(8):
            self.bubbles.append(Bubble(self._rng.random() * WORLD_W, 1.0 + self._rng.random() * (WORLD_H - 2.0), self._rng.random(), 0.045 + self._rng.random() * 0.08))

    def add_fish(self, x: float, y: float, z: float, direction: int) -> Fish:
        fish = Fish(
            max(1.0, min(WORLD_W - 2.0, x)),
            max(1.0, min(WORLD_H - 2.0, y)),
            max(0.0, min(1.0, z)),
            self._rng.uniform(-0.0016, 0.0016) or 0.0011,
            1 if direction >= 0 else -1,
            0.055 + self._rng.random() * 0.09,
            self._rng.random() * 6.283185,
        )
        self.fish.append(fish)
        return fish

    def step(self) -> None:
        self.tick += 1
        for i, fish in enumerate(self.fish):
            fish.phase += 0.045 + fish.speed * 0.08
            fish.x += fish.direction * fish.speed * (0.7 + fish.z * 0.65)
            fish.y += 0.018 * __import__("math").sin(fish.phase)
            fish.z += fish.dz
            if fish.z <= 0.06:
                fish.z, fish.dz = 0.06, abs(fish.dz)
            elif fish.z >= 0.98:
                fish.z, fish.dz = 0.98, -abs(fish.dz)
            if self.tick % 700 == i * 43:
                fish.dz = max(-0.003, min(0.003, fish.dz + self._rng.uniform(-0.0006, 0.0006)))
            if fish.x <= 1.0:
                fish.x, fish.direction = 1.0, 1
            elif fish.x >= WORLD_W - 2.0:
                fish.x, fish.direction = WORLD_W - 2.0, -1
            fish.y = max(1.0, min(WORLD_H - 3.0, fish.y))
        for bubble in self.bubbles:
            bubble.y -= bubble.speed * (0.75 + bubble.z * 0.5)
            if bubble.y < 0.5:
                bubble.y = WORLD_H - 1.5
                bubble.x = (bubble.x + 11.0 + self._rng.random() * 17.0) % WORLD_W
                bubble.z = self._rng.random()
        if self.fish:
            focus = sorted(self.fish, key=lambda fish: fish.z, reverse=True)[:4]
            target_x = sum(fish.x for fish in focus) / len(focus) - VIEW_W * 0.5
            target_y = sum(fish.y for fish in focus) / len(focus) - VIEW_H * 0.5
            self.cam_x += (max(0.0, min(WORLD_W - VIEW_W, target_x)) - self.cam_x) * 0.025
            self.cam_y += (max(0.0, min(WORLD_H - VIEW_H, target_y)) - self.cam_y) * 0.025

    @staticmethod
    def _plot(fb: list[list[int]], x: int, y: int, value: int) -> None:
        if 0 <= x < VIEW_W and 0 <= y < VIEW_H:
            fb[y][x] = max(fb[y][x], max(0, min(255, value)))

    def render(self) -> list[list[int]]:
        fb = [[0] * VIEW_W for _ in range(VIEW_H)]
        # Sparse scenery is intentionally dim; fish own the visual hierarchy.
        for x, y, z in self.rocks:
            vx, vy = round(x - self.cam_x), round(y - self.cam_y)
            c = int(self.bright * (0.12 + z * 0.20))
            self._plot(fb, vx, vy, c)
            if z > 0.55:
                self._plot(fb, vx + 1, vy, c // 2)
        for x, y, z in self.plants:
            vx, vy = round(x - self.cam_x), round(y - self.cam_y)
            c = int(self.bright * (0.10 + z * 0.18))
            for stem in range(1 + int(z * 3.0)):
                self._plot(fb, vx + (stem & 1), vy - stem, c)
        for bubble in self.bubbles:
            vx, vy = round(bubble.x - self.cam_x), round(bubble.y - self.cam_y)
            self._plot(fb, vx, vy, int(self.bright * (0.10 + bubble.z * 0.24)))
        # Far fish first. Near fish are brighter and gain body/tail pixels.
        for fish in sorted(self.fish, key=lambda item: item.z):
            x, y = round(fish.x - self.cam_x), round(fish.y - self.cam_y)
            c = int(self.bright * (0.28 + fish.z * 0.72))
            d = fish.direction
            self._plot(fb, x, y, c)
            self._plot(fb, x - d, y, c // 2)
            if fish.z >= 0.34:
                self._plot(fb, x + d, y, c * 3 // 4)
            if fish.z >= 0.67:
                self._plot(fb, x, y - 1, c // 2)
                self._plot(fb, x - 2 * d, y - 1, c // 3)
                self._plot(fb, x - 2 * d, y + 1, c // 3)
        return fb

    def px_hex(self) -> str:
        return "".join(f"{value:02x}" for row in self.render() for value in row)

    def state(self) -> dict:
        return {
            "ok": True,
            "sim": "aquarium",
            "w": VIEW_W,
            "h": VIEW_H,
            "world": [int(WORLD_W), int(WORLD_H)],
            "fish": len(self.fish),
            "plants": len(self.plants),
            "rocks": len(self.rocks),
            "bubbles": len(self.bubbles),
            "cam": [round(self.cam_x, 1), round(self.cam_y, 1)],
            "tick": self.tick,
            "imu": False,
            "ldr": False,
            "source": "twin",
        }
