# Print + buy

This is the **Soldered / e-radionica Maker LEDDisplay2** — assembled 32×9 Charlieplex (2× IS31FL3731, ESP8266). Not Adafruit’s 16×9 breakout. Not Inkplate / House Wall e-ink. Not HUB75.

## Buy the board

In-stock path as of 2026:

- **Crowd Supply (Soldered)** — [Maker LED Display](https://www.crowdsupply.com/soldered/maker-led-display)
  - **Display2** = 32×9 (this firmware)
  - Display4 = 64×9 (wrong size for this repo)
  - Advanced kit adds USB-UART programmer + headers (~$39 + shipping)
- Hardware + official 2019 3D kit: [SolderedElectronics/Maker-Display](https://github.com/SolderedElectronics/Maker-Display)

Soldered’s main shop rotates Inkplate first; Crowd Supply is still the reliable SKU page.

## Our case (print this)

Two-piece PETG mule that **clears a soldered 1×6 programming header**. Official 2019 Small Top/Bottom will not close over that header.

| File | Role |
| --- | --- |
| [`case/maker-led-display2-case-base.stl`](case/maker-led-display2-case-base.stl) | tray / ports |
| [`case/maker-led-display2-case-bezel.stl`](case/maker-led-display2-case-bezel.stl) | face + LED window + header poke |
| [`case/maker-led-display2-case-v0.1.1.zip`](case/maker-led-display2-case-v0.1.1.zip) | both STLs |
| [`case/maker-led-display2-case.scad`](case/maker-led-display2-case.scad) | source |

Print: PETG, P1S, no supports expected. Base floor-down. Bezel **face on the bed**. v0.1.1 is fit-tested on a live v2.2 with header.

## Official 2019 kit (Soldered)

Mirrored from their repo for convenience — **their files, their license**. Kickstand legs if you do **not** have a tall programming header.

- [`official/`](official/) — Small Top / Bottom / Leg / washer / plexi DXF
- Source zip: `Maker_Display_3D_files.zip` from [Maker-Display](https://github.com/SolderedElectronics/Maker-Display)
