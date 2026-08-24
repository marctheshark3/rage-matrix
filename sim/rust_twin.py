#!/usr/bin/env python3
"""Persistent matrix-sim serve process. Python twin is the fallback."""
from __future__ import annotations

import json
import os
import subprocess
import threading
from pathlib import Path

BIN = Path(
    os.environ.get(
        "MATRIX_SIM_BIN",
        str(Path.home() / "Documents/the-grid/maker-led-display/rust/matrix-sim/target/release/matrix-sim"),
    )
)

_lock = threading.Lock()
_proc: subprocess.Popen | None = None


def available() -> bool:
    return BIN.is_file() and os.access(BIN, os.X_OK)


def _boot() -> subprocess.Popen:
    return subprocess.Popen(
        [str(BIN), "serve"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )


def _line(cmd: str) -> str:
    global _proc
    if _proc is None or _proc.poll() is not None:
        _proc = _boot()
    assert _proc.stdin and _proc.stdout
    _proc.stdin.write(cmd.rstrip() + "\n")
    _proc.stdin.flush()
    out = _proc.stdout.readline()
    if not out:
        raise RuntimeError("matrix-sim serve died")
    return out.strip()


def send(cmd: str) -> str:
    with _lock:
        try:
            return _line(cmd)
        except Exception:
            global _proc
            if _proc is not None:
                try:
                    _proc.kill()
                except Exception:
                    pass
            _proc = None
            return _line(cmd)


def pull(view: str) -> tuple[dict, str]:
    send(view)
    send("step")
    raw = send("jsonpx")
    if "\t" in raw:
        js, px = raw.split("\t", 1)
    else:
        js, px = raw, send("px")
    st = json.loads(js)
    st["source"] = "rust"
    st["sim"] = view
    return st, px


def cmd(view: str, name: str, data: dict | None = None) -> dict:
    data = data or {}
    send(view)
    if name == "seed":
        send("seed")
    elif name == "next":
        send("next")
    elif name == "wall":
        send(f"wall {int(data.get('x') or 16)} {int(data.get('y') or 4)}")
    elif name == "west":
        send("west arty" if str(data.get("arty", "0")) not in ("0", "false", "") else "west")
    elif name == "east":
        send("east arty" if str(data.get("arty", "0")) not in ("0", "false", "") else "east")
    elif name == "feed":
        x, y = data.get("x"), data.get("y")
        if x not in (None, "") and y not in (None, ""):
            send(f"feed {int(x)} {int(y)}")
        else:
            send("feed")
    elif name == "scatter":
        send("scatter")
    elif name == "shake":
        send(f"shake {float(data.get('amp') or 1.2)}")
    elif name == "hunt":
        send("hunt")
    st, _ = pull(view)
    return st
