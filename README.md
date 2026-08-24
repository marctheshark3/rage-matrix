# Rage Matrix

Firmware, host twins, and a small HTML hub for the **Maker LEDDisplay2** — a 32×9 greyscale Charlieplex desk panel (2× IS31FL3731 on an ESP8266).

The 32×9 is a **camera** into a larger world. Two sims stay resident in RAM; switching modes does not wipe them.

- **Tank** — 64×18 torus. Tadpole prey vs chevron hunters. Always-swim, graze-and-go, hunter-lag camera.
- **War** — 48×18 bounded field. Default civ campaign: CAMP → MELEE → GUNS → ARTY, then they shoot. Match end flashes `W3-E5` then `NEXT`.
- **Reel** — wave, fire, plasma, life, stars, … Each mode flashes a title card (`WAR ZONE`, `TANK`, `FIRE`, …) then starts.

Not a HUB75 wall. Not e-ink. No IMU / LDR on the v2.2 PCB — ADC is the LiPo divider.

## Hardware

- **Buy:** [Crowd Supply — Maker LED Display](https://www.crowdsupply.com/soldered/maker-led-display) (pick **Display2 / 32×9**)
- **Official hardware repo:** [SolderedElectronics/Maker-Display](https://github.com/SolderedElectronics/Maker-Display)
- **Print our case:** [`printables/`](printables/) — two-piece PETG that clears a soldered 1×6 header (`case/*.stl`). Official 2019 Small kit is also mirrored there.

## Drive it

```text
POST /mode          name=war|tank|fire|wave|…
GET  /mode.json     current mode + title leftover
GET  /panel.json    pixels actually on the glass
GET  /tank.json     /fb.json
GET  /war.json      /war/fb.json
POST /tank/feed|scatter|shake|seed|hunt|focus
POST /war/west|east|next|seed|wall|focus
HTTP OTA            POST /update  field=firmware
```

Serial @ 9600: `e` tank · `r` war · `f` feed · `z` shake · `a` reel · `n` next · `0-9` lock · `bN` brightness.

Hub (this repo):

```bash
export MATRIX_URL=http://rage-matrix.local   # or the board IP
python3 hub/server.py                        # http://127.0.0.1:8765
```

## Flash

```bash
cp src/secrets.h.example src/secrets.h   # SSID / password stay local
pio run -e maker_led_display2
curl -F "firmware=@.pio/build/maker_led_display2/firmware.bin" \
     http://rage-matrix.local/update
```

ArduinoOTA UDP is silent on this board. Use HTTP. The panel is dark ~15s after `Update Success`.

RAM ~52% / flash ~37% on the current image.

## Host twins

Firmware stays **C++** (Xtensa). The algorithm also lives in:

| Path | Role |
| --- | --- |
| `sim/critters.py` `sim/war.py` | Python twins (fallback) |
| `rust/matrix-sim` | no-heap Rust crate — ~70–85× the Python twin |

```bash
python3 -c "from sim.critters import Tank; from sim.war import War; t=Tank(); t.seed(); w=War(); w.seed(); [t.step() for _ in range(40)]; [w.step() for _ in range(40)]; print('ok', t.state()['prey'], w.state()['west'])"

export PATH="$HOME/.rustup/toolchains/1.95.0-aarch64-unknown-linux-gnu/bin:$HOME/.cargo/bin:$PATH"
cd rust/matrix-sim && cargo test --release && cargo run --release -- bench
```

I²C still caps the glass at ~25–30 fps. Faster host math does not raise that.

## Layout

```
src/           ESP8266 firmware (Arduino / PlatformIO)
lib/           IS31FL3731 driver
sim/           Python twins + rust_twin stdin bridge
rust/matrix-sim
hub/           standalone HTMX desk + API proxy
```

`src/secrets.h` is gitignored. Never commit a live SSID.

## License

MIT — Rage Industries.
