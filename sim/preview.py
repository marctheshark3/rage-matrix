#!/usr/bin/env python3
"""Render firmware-faithful preview video of Maker LEDDisplay2.

Usage:
  python3 -m sim.preview                 # default 18s reel → preview/preview.mp4
  python3 -m sim.preview --mode wave --seconds 8
  python3 -m sim.preview --gif
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from sim.engine import Display2Sim
from sim.render_board import render_frame


def encode_mp4(frames_dir: Path, out: Path, fps: int) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        "ffmpeg", "-y",
        "-framerate", str(fps),
        "-i", str(frames_dir / "f%05d.png"),
        "-c:v", "libx264",
        "-pix_fmt", "yuv420p",
        "-crf", "18",
        "-movflags", "+faststart",
        str(out),
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def encode_gif(mp4: Path, gif: Path) -> None:
    pal = gif.with_suffix(".palette.png")
    subprocess.run(
        ["ffmpeg", "-y", "-i", str(mp4), "-vf", "fps=12,scale=640:-1:flags=lanczos,palettegen", str(pal)],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    subprocess.run(
        ["ffmpeg", "-y", "-i", str(mp4), "-i", str(pal),
         "-lavfi", "fps=12,scale=640:-1:flags=lanczos[x];[x][1:v]paletteuse",
         str(gif)],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    pal.unlink(missing_ok=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["wave", "plasma", "sine", "reel"], default="reel")
    ap.add_argument("--seconds", type=float, default=0.0, help="per-mode seconds (reel ignores)")
    ap.add_argument("--fps", type=int, default=24)
    ap.add_argument("--gif", action="store_true")
    ap.add_argument("--out", default="")
    args = ap.parse_args()

    preview = ROOT / "preview"
    frames = preview / "_frames"
    if frames.exists():
        for p in frames.glob("*.png"):
            p.unlink()
    frames.mkdir(parents=True, exist_ok=True)

    if args.mode == "reel":
        timeline = [("wave", 7.0), ("sine", 5.0), ("plasma", 5.0)]
    else:
        secs = args.seconds or 8.0
        timeline = [(args.mode, secs)]

    sim = Display2Sim()
    sim.seed()
    n = 0
    for mode, secs in timeline:
        sim.seed()
        total = int(secs * args.fps)
        for i in range(total):
            px = sim.tick(mode, dt_ms=int(1000 / args.fps))
            cap = f"Maker LEDDisplay2  ·  32×9  ·  mode={mode}  ·  sparse preview"
            img = render_frame(px, caption=cap)
            img.save(frames / f"f{n:05d}.png")
            n += 1

    stem = args.out or str(preview / ("preview.gif" if args.gif and args.mode != "reel" else "preview.mp4"))
    out = Path(stem)
    if out.suffix.lower() == ".gif":
        tmp = preview / "preview.mp4"
        encode_mp4(frames, tmp, args.fps)
        encode_gif(tmp, out)
    else:
        encode_mp4(frames, out, args.fps)
        if args.gif:
            encode_gif(out, out.with_suffix(".gif"))

    print(out)
    print(f"frames={n} fps={args.fps}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
