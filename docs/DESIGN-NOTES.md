# Design notes: parts, wiring, and what "3D" means

The buildable parts lists now live in `../recipes/` and `../parts.json`.
This page keeps the reasoning behind them.

Goal: captions that appear on the side the speaker is on, in a colour and
number per speaker (anonymous), driven by
HearAura on an iPhone.

Prices were looked up on 2026-11-14 where a source is given, otherwise they
are estimates. Check before ordering.

## How the pieces divide the work

- **Display glasses, bought used.** A wide, two-eye display is the one part
  not worth building. HearAura already draws captions on any display plugged
  into the iPhone.
- **A sensor pod you build**, clipped to the glasses: four microphones find
  the direction of a voice, a motion sensor knows which way your head points,
  and a small Bluetooth computer reports both to the iPhone ten times a
  second (see PROTOCOL.md in this folder).
- **The iPhone** turns speech into text, numbers the speakers by direction,
  and draws each caption on the right side of the display.

## Option A: spatial captions (recommended)

| Part | Why | Source | Price |
|---|---|---|---|
| Used display glasses: Rokid Max, Xreal Air or Viture | Two-eye 1080p display, about 45 degrees wide, on a USB-C cable | eBay, local ads | $120 to $180 |
| Seeed XIAO ESP32-S3 (the Sense version adds a camera) | Bluetooth LE, two I2S buses for four microphones, battery charging | DigiKey 113991115 (Sense) | $13.90 |
| 4 x INMP441 I2S microphone modules | Two at the front of each temple, two at the hinges: a rectangle that hears all the way round | Tayda, Electropeak, Amazon | $2.50 to $4 each |
| Adafruit BNO085 9-axis motion sensor (4754) | Head direction, already fused on the chip | Adafruit, DigiKey | $24.95 to $29.50 |
| LiPo battery, 400 mAh, with protection | About 5 hours | Amazon multi-pack | $3 to $6 |
| Thin wire, slide switch, 3D-printed clips | | | $5 |

Pod total about $60 to $70; with used glasses about $180 to $250.

## Option B: less soldering

| Part | Why | Source | Price |
|---|---|---|---|
| Seeed reSpeaker Flex XVF3800 Linear-4 with XIAO ESP32S3 | Four microphones on a strip (about 11 cm, joined by a 20 cm ribbon) plus a chip that already finds voice direction and cleans up noise | Seeed | $64.99 |
| Adafruit BNO085 | Head direction | Adafruit | $29.50 |
| Battery, wire, clips | | | $10 |
| Used display glasses | As above | | $120 to $180 |

The firmware supports this board and the guide has lessons for it, but nobody
has built it: see the notes in `recipes/respeaker-flex/recipe.json` for what is
still unchecked. A straight line of microphones cannot tell in front from
behind.

## Option C: all homemade, with the camera

This is the `one-eye` recipe. The guide at
https://hearaura.app/build-glasses/ builds it step by step.

One board does everything: the Seeed XIAO ESP32-S3 Sense carries the
camera, so sound, head direction and faces are fused on the glasses and only
numbers go to the iPhone. The firmware in `../firmware/` is written for this build.

| Part | Why | Source | Price |
|---|---|---|---|
| Seeed XIAO ESP32-S3 Sense | Bluetooth, camera (OV2640, about 66 degrees wide), 8 MB PSRAM for face detection, battery charging | DigiKey 113991115 | $13.90 |
| 0.96" OLED 128x64, I2C | The one-eye display | Seeed Grove OLED 0.96" | $6.00 |
| Plano-convex lens, 20 mm, about 50 mm focal length | Lets the eye focus on the OLED | Amazon, eBay | about $5 |
| First-surface mirror, about 25x20 mm | Folds the light path | Amazon multi-pack | about $8 |
| Beam splitter 30x30 mm, 50/50 | The see-through piece | eBay | $15 to $19 |
| 4 x INMP441 I2S microphone modules | Voice direction all the way round | Tayda, Electropeak, Amazon | $2.50 to $4 each |
| Adafruit BNO085 (4754) | Head direction | Adafruit, DigiKey | $24.95 to $29.50 |
| LiPo battery, 500 mAh, with protection | See battery life below | Amazon multi-pack | $3 to $6 |
| Wire, slide switch, 3D-printed housing and clips | | | $5 |
| Optional: wide-angle (120 to 160 degree) OV2640 lens module | Sees faces further to the side | Amazon, AliExpress | about $8 |

Total about $95 to $110.

### What the camera adds, and what it does not

- **Height and distance for speakers you are facing.** When a voice's
  direction matches a face the camera sees, the report carries that face's
  exact direction, how far above or below it is, and a rough distance from
  the size of the face. That is the third dimension sound alone cannot give.
- **Front or behind, settled.** Two of the four microphones are on a second
  input that starts a few samples off; the firmware has a calibration value
  for it. A face in view confirms "in front" without relying on that.
- **Only within the camera's view.** The stock lens sees about 66 degrees.
  Someone beside or behind you is still placed by sound alone.
- **Only angles leave the glasses.** Pictures are analysed on the board and
  dropped. Nothing is stored or sent, and nobody is recognised.
- **Not on the one-eye display.** A 128x64 OLED can show the speaker's
  number and an arrow (left, ahead, right, behind), not a caption floating
  at the speaker. The height and distance are there for a better display
  later, and for the iPhone to tell near voices from far ones.

### Costs of the camera

- **Battery.** Camera plus face detection roughly doubles the draw. Expect
  about 2 hours from 500 mAh, against 4 to 5 without. An estimate, not a
  measurement.
- **Heat and weight** on one temple, and a visible lens pointed at people.
  Tell the people you are with, and cover it where cameras are unwelcome.
- **Firmware version.** Face detection uses Espressif's esp-dl, bundled
  with Arduino-ESP32 2.0.x for this board but not 3.x. `platformio.ini`
  pins the right one.

### Wiring

| Signal | XIAO pin | Goes to |
|---|---|---|
| I2C data, clock | D4, D5 | OLED and BNO085 (shared) |
| Microphone clock, word select | D0, D1 | All four microphones |
| Front pair data | D2 | Front-left (L/R pin to ground) and front-right (L/R pin to 3V3) |
| Rear pair data | D10 | Rear-left and rear-right, same L/R arrangement |
| 3V3, GND | 3V3, GND | Everything |
| Battery | BAT pads on the back | LiPo through the slide switch |

Do not fit a microSD card: D10 shares its slot's wiring.

## What "3D" honestly means with these parts

- **Left, right, front, behind: yes.** Four microphones in a rectangle about
  14 cm wide resolve direction around you to roughly 10 to 15 degrees in a
  quiet room, worse in noise or echo.
- **Up and down: weakly.** It needs a microphone above or below the others,
  and a glasses frame gives about 3 cm.
- **Distance: not from sound.** It takes a camera that sees the face (the
  XIAO Sense has one) or an assumed distance. This is stage two.
- **Who it is: never.** Speakers are numbered by where they are. Combining
  that with HearAura's voice-based "Tell voices apart" is a later step.

## Not built or tested

No hardware has been built. The direction maths (`doa.h`) passes its test
on simulated sound, where it is accurate to under a degree; a real room
with echo and noise will be far worse, and that has not been measured. The
firmware builds without errors but has never run on a board.
