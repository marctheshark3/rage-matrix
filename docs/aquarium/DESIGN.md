---
version: alpha
name: Rage Aquarium
description: 32x9 camera into a sealed 56x18 monochrome aquarium. Still water, side-to-side fish, z as size and light.
colors:
  primary: "#1C1B18"
  secondary: "#6B6760"
  tertiary: "#E31C23"
  neutral: "#F4F1EA"
  silk: "#F4F1EA"
  chassis: "#3A3A3A"
  screen: "#0B0B0B"
  charcoal: "#2A2A2A"
  water: "#000000"
  far: "#3A3A3A"
  mid: "#9AA0A4"
  near: "#F4F1EA"
typography:
  h1:
    fontFamily: IBM Plex Sans
    fontSize: 2rem
    fontWeight: 600
    lineHeight: 1.15
    letterSpacing: "-0.02em"
  body-md:
    fontFamily: IBM Plex Sans
    fontSize: 1rem
    fontWeight: 400
    lineHeight: 1.45
  label-caps:
    fontFamily: IBM Plex Mono
    fontSize: 0.75rem
    fontWeight: 400
    letterSpacing: "0.08em"
rounded:
  sm: 4px
  md: 10px
spacing:
  sm: 8px
  md: 16px
  lg: 24px
components:
  button-primary:
    backgroundColor: "{colors.primary}"
    textColor: "{colors.silk}"
    rounded: "{rounded.sm}"
    padding: 12px
  button-primary-hover:
    backgroundColor: "{colors.chassis}"
    textColor: "{colors.silk}"
  panel-screen:
    backgroundColor: "{colors.screen}"
    textColor: "{colors.silk}"
    rounded: "{rounded.md}"
    padding: 16px
  panel-paper:
    backgroundColor: "{colors.neutral}"
    textColor: "{colors.primary}"
    rounded: "{rounded.sm}"
    padding: 16px
  panel-charcoal:
    backgroundColor: "{colors.charcoal}"
    textColor: "{colors.silk}"
    rounded: "{rounded.sm}"
    padding: 16px
  hanko:
    backgroundColor: "{colors.tertiary}"
    textColor: "{colors.primary}"
    rounded: "{rounded.sm}"
    padding: 8px
  led-water:
    backgroundColor: "{colors.water}"
    textColor: "{colors.near}"
    rounded: "{rounded.sm}"
    padding: 4px
  led-far:
    backgroundColor: "{colors.far}"
    textColor: "{colors.near}"
    rounded: "{rounded.sm}"
    padding: 4px
  led-mid:
    backgroundColor: "{colors.mid}"
    textColor: "{colors.primary}"
    rounded: "{rounded.sm}"
    padding: 4px
  led-near:
    backgroundColor: "{colors.near}"
    textColor: "{colors.primary}"
    rounded: "{rounded.sm}"
    padding: 4px
---

## Overview

**Room:** Rage. **Palette:** Rage plate lock — red, black, gray, white silk-screen. Hinomaru / hanko / ofuda energy.

**Lede:** Outsource the thinking. Keep the understanding.

The 32x9 Maker LEDDisplay2 is a **camera**, not a tank wall. Aquarium is a still-water diorama: fish cruise left/right in a larger bounded volume. Depth is the only special effect. The glass stays empty enough that a single near fish can own the frame.

This is not Tank. Tank is a torus petri (prey, hunters, trails, 8-way heading, feed/shake). Aquarium is sealed, bounded, no HUD, no titles on the live field, no metabolism.

**Surface:** Inspect. One object — the camera into water.

**Hot:** the board's one physical red LED. Never a hue inside the 288-pixel field.

**Costume:** none.

## Colors

Glass is greyscale. Spec-sheet chrome uses the Rage plate. Do not put red, cream, or orange into the framebuffer.

| Token | Hex | Job |
| --- | --- | --- |
| water | `#000000` | Off LED. Negative space is the product. |
| far | `#3A3A3A` | Distant fish / dim scenery |
| mid | `#9AA0A4` | Mid-depth body |
| near | `#F4F1EA` | Near fish peak (silk, not `#FFFFFF`) |
| tertiary | `#E31C23` | Spec-sheet hanko only. Not on glass. |
| primary | `#1C1B18` | Ink on paper |
| secondary | `#6B6760` | Mute labels |
| silk / neutral | `#F4F1EA` | Paper and silk type |

Firmware mapping at master brightness `B` (default 90):

```
fish    c = B * (0.28 + z * 0.72)
rock    c = B * (0.12 + z * 0.20)
plant   c = B * (0.10 + z * 0.18)
bubble  c = B * (0.10 + z * 0.24)
```

Plot is max-blend. Near fish always win collisions. Water stays 0.

## Typography

IBM Plex Sans for spec prose. IBM Plex Mono for unit labels. **No type on the 32x9 live field.** Mode title card (`AQUARIUM`, 2200 ms) is firmware chrome before the sim paints — never during `aquariumRender`.

## Layout

World `56 x 18` world-units. View `32 x 9` LEDs. One LED = one world-unit. Origin top-left. `y` increases downward. `z` in `[0, 1]` where `1` is nearest the glass.

```
world  0 ---------------- 56
view       [cam_x .... +32]
height 18  camera y in [0, 9]
```

No wrap. Walls bounce. Floor is scenery, not a HUD rule.

## Elevation & Depth

`z` is the only elevation system.

| Band | z | Glyph | Peak vs B |
| --- | --- | --- | --- |
| FAR | `z < 0.34` | 2 px: body + tail | ~0.28–0.52 |
| MID | `0.34 <= z < 0.67` | 3 px: tail, body, nose | ~0.52–0.76 |
| NEAR | `z >= 0.67` | 6 px: MID + dorsal + fork | ~0.76–1.00 |

Rocks gain a second pixel only when `z > 0.55`. Plant stem count is `1 + int(z * 3)` (1–4). Bubbles stay 1 px.

Fish speed scales with depth: `dx = dir * speed * (0.70 + z * 0.65)`. Near fish move more. That sells scale without extra pixels.

## Shapes

Hard pixels. No anti-alias, no glow, no trail decay, no 8-way chevrons.

`d = direction` (`+1` right, `-1` left). Head is at `(x, y)`.

```
FAR   (z < 0.34)           MID  (z >= 0.34)
  t B                        t B n
  · ·                        · · ·

NEAR  (z >= 0.67)
      D
  f   B   n
  f

B body  t tail=x-d     n nose=x+d
D dorsal=(x, y-1)      f fork=(x-2d, y±1)
```

Rock: `(x,y)` plus optional `(x+1,y)` at half value.

Plant: stem up from floor, stagger `x + (stem & 1)`.

Bubble: single pixel.

## Components

**Fish (12).** Side-to-side with a slow bounded z-drift. Soft sine on `y` (`0.018 * sin(phase)`). Bounce at `x=1` and `x=WORLD_W-2`; bounce z inside `[0.06, 0.98]`. Persist across mode switches. Never reseed in `enterMode`.

**Camera.** Follow centroid of the **4 nearest** fish (`highest z`). Lerp `0.025` per tick. Clamp:

```
cam_x in [0, 56-32]
cam_y in [0, 18-9]
```

Manual pan (`/aquarium/pan`) is a nudge inside the same clamp. No wrap, no hunter-lag, no shake rim.

**Scenery.** Plants sit on `y = WORLD_H-1`. Rocks on the same floor. Bubbles rise (`y` decreases) and recycle at the floor with a new `x` and `z`.

**Object budget (seed / hard cap).**

| Kind | Seed | Cap | Notes |
| --- | --- | --- | --- |
| fish | 12 | 12 | No spawn after begin |
| plants | 6 | 8 | Static |
| rocks | 5 | 8 | Static |
| bubbles | 8 | 12 | Recycle, never grow |

Occupied LEDs in a typical frame should stay under ~40/288. If a frame is busier than that, scenery is too loud.

**Seal.** `aquariumBegin` / `seed_world` is the only automatic seed. Mode entry must not call it. No feed, scatter, hunt, or shake verbs.

## Do's and Don'ts

**Do**

- Treat 32x9 as a window into 56x18.
- Let empty water dominate.
- Scale brightness and glyph size from `z` only.
- Draw far-to-near so near fish overwrite.
- Keep twins (Python / firmware) pixel-lockstep on glyphs and lerp.
- Stay on Rage plate for any hub/spec chrome.

**Never list**

- Text, scores, clocks, names, or title cards on the live field.
- Tank language: torus wrap, trails, prey/pred, 8-way heading, energy, feed, shake, rim flash.
- War language: camps, shots, `W3-E5`, NEXT.
- Second hue on glass (no red fish, no cyan water).
- Yellow Publish, cream/orange brand, Pocket Operator chrome.
- Teenage Engineering names or marks.
- LT-1 owner chrome mixed into this room.
- Kid-face publish, factory hopper, Northrop paths.
- Discord gateway, merge, flash, push, X post, ads, email-dad.
- Visual clutter: schools filling the row, plants taller than 4 px, rocks as walls, bubble rain.

## Acceptance tests

Hand these to proto. Existing `tests/test_aquarium.py` already covers 1–5 and 8.

1. **World larger than view.** `(72, 18) > (32, 9)`.
2. **Deterministic seed.** Same seed → same `state()` and `px_hex()` at t=0.
3. **Side-to-side, bounded.** After 240 steps some fish moved `>1` unit; all `x` in `[1, 70]`; `direction` in `{-1, +1}` only.
4. **Depth = brighter + bigger.** Isolated far `z=0.05` vs near `z=0.95` in the same frame: `near_peak > far_peak` and near occupies more lit pixels.
5. **Camera bounded.** Driving fish to the right edge increases `cam_x` but keeps `cam` inside `[0, 40] x [0, 9]`.
6. **Budget.** `0 < plants <= 8`, `0 < rocks <= 8`, `0 < bubbles <= 12`, `fish == 9` after seed.
7. **No text on glass.** `aquariumRender` writes only plot values; no font, no `cardShow` inside the sim tick.
8. **Seal.** `enterMode` does not contain `aquariumSeed` / `crittersBegin` / `warSeed`. Hub `pull_fb("aquarium")` returns `sim=aquarium`, not tank.
9. **Distinct from tank.** Hex dump of aquarium vs tank at t=40 differs; aquarium framebuffer has no trail decay field; no 8-neighbor chevron.
10. **Sparsity.** Mean lit pixels over 120 seeded frames `< 40`. Peak `< 70`.
11. **Glyph lock.** FAR never plots nose or dorsal. MID plots nose, not fork. NEAR plots all six cells when on-screen.
12. **Palette.** Live px values are greyscale levels. Spec HTML uses only Rage plate tokens. Zero `#FFD400` / Publish yellow.

## Tank vs aquarium (do not reopen)

| | Tank | Aquarium |
| --- | --- | --- |
| World | 64x18 torus | 72x18 box |
| Actors | prey + hunters | fish only |
| Motion | 8-way heading | ±X cruise + tiny sine Y |
| Depth | none (energy = bright) | z → size + bright + speed |
| FX | trails, shake rim | dim rocks/plants, 1px bubbles |
| Camera | hunter-lag, wrap | 4-nearest lerp, clamp |
| Verbs | feed/shake/hunt/seed | pan/focus only |
| Mood | petri / hunger | still water / glance |
