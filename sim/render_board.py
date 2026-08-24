"""Paint a 32x9 Maker LEDDisplay2-like panel. One cell = one LED."""
from __future__ import annotations

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

from .engine import H, W

# Physical-ish: two 16x9 charlieplex halves, ~4.0 mm pitch → 128 x 36 mm
PITCH = 36
MARGIN_X = 48
MARGIN_Y = 56
LED_R = 11
BOARD_RGB = (18, 20, 26)
SILK = (42, 48, 62)
LED_PALETTES = {
    "white": ((230, 236, 255), (22, 24, 30)),
    "amber": ((255, 176, 48), (28, 20, 10)),
    "violet": ((196, 132, 255), (22, 16, 32)),
    "cyan": ((80, 230, 255), (12, 24, 30)),
    "rage": ((255, 72, 64), (28, 12, 12)),
}


def board_size() -> tuple[int, int]:
    return (
        MARGIN_X * 2 + W * PITCH,
        MARGIN_Y * 2 + H * PITCH,
    )


def _font(size: int) -> ImageFont.ImageFont:
    for path in (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    ):
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


def ImageChops_screen(a: Image.Image, b: Image.Image) -> Image.Image:
    aa = np.asarray(a, dtype=np.float32) / 255.0
    bb = np.asarray(b, dtype=np.float32) / 255.0
    out = 1.0 - (1.0 - aa) * (1.0 - bb)
    return Image.fromarray(np.clip(out * 255.0, 0, 255).astype(np.uint8), "RGB")


def render_frame(
    pixels: list[list[int]],
    caption: str = "",
    color: str = "white",
    footer: str = "",
) -> Image.Image:
    on, off = LED_PALETTES.get(color, LED_PALETTES["white"])
    bw, bh = board_size()
    canvas_w, canvas_h = 1280, 720
    canvas = Image.new("RGB", (canvas_w, canvas_h), (8, 8, 10))
    board = Image.new("RGB", (bw, bh), BOARD_RGB)
    draw = ImageDraw.Draw(board)

    draw.rounded_rectangle((4, 4, bw - 5, bh - 5), radius=14, outline=SILK, width=2)
    seam_x = MARGIN_X + 16 * PITCH
    draw.line((seam_x, 18, seam_x, bh - 18), fill=(28, 32, 40), width=2)

    glow = Image.new("RGB", (bw, bh), (0, 0, 0))
    gdraw = ImageDraw.Draw(glow)

    for y in range(H):
        for x in range(W):
            cx = MARGIN_X + x * PITCH + PITCH // 2
            cy = MARGIN_Y + y * PITCH + PITCH // 2
            v = max(0, min(255, int(pixels[y][x])))
            t = v / 255.0
            draw.ellipse(
                (cx - LED_R, cy - LED_R, cx + LED_R, cy + LED_R),
                fill=off,
                outline=(16, 16, 20),
            )
            if t <= 0.01:
                continue
            col = tuple(int(off[i] + (on[i] - off[i]) * t) for i in range(3))
            draw.ellipse(
                (cx - LED_R + 1, cy - LED_R + 1, cx + LED_R - 1, cy + LED_R - 1),
                fill=col,
            )
            br = int(LED_R + 10 + 18 * t)
            bloom = tuple(int(c * t * 0.55) for c in on)
            gdraw.ellipse((cx - br, cy - br, cx + br, cy + br), fill=bloom)

    glow = glow.filter(ImageFilter.GaussianBlur(radius=7))
    board = Image.blend(board, ImageChops_screen(board, glow), 0.85)

    max_w, max_h = canvas_w - 80, canvas_h - 140
    scale = min(max_w / bw, max_h / bh)
    nw, nh = int(bw * scale), int(bh * scale)
    board = board.resize((nw, nh), Image.Resampling.LANCZOS)
    ox = (canvas_w - nw) // 2
    oy = (canvas_h - nh) // 2 - 10
    canvas.paste(board, (ox, oy))

    cd = ImageDraw.Draw(canvas)
    font = _font(22)
    small = _font(16)
    label = caption or "RAGE INDUSTRIES  ·  32×9  ·  firmware-faithful"
    cd.text((40, 28), label, fill=(200, 206, 220), font=font)
    cd.text(
        (40, canvas_h - 48),
        footer
        or f"RAGE INDUSTRIES  ·  demo palette={color}  ·  live PCB is cool-white",
        fill=(110, 116, 128),
        font=small,
    )
    return canvas
