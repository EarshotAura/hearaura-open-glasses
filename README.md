# HearAura open glasses

Glasses you build yourself that show live captions, and, with a few more
parts, tell where each speaker is.

The first app that drives them is [HearAura](https://hearaura.app) on
iPhone. The link between app and glasses is open and small, so they are not
tied to it: any phone, laptop or pocket computer can send them text, and
the board can do more of its own thinking over time. See
[`hosts/`](hosts/README.md) and [`docs/PROTOCOL.md`](docs/PROTOCOL.md).

**Want to build a pair?** Use the interactive guide, Caption Glasses 101:
https://hearaura.app/build-glasses/ . It asks what parts you have, picks a
recipe, and walks you through it one small step at a time.

**Built something different?** Add it as a recipe, so the guide can offer it
to the next person. See [CONTRIBUTING.md](CONTRIBUTING.md).

## Recipes

A recipe is one way to build the glasses: a list of parts and the lessons
that put them together. The guide reads them from this repository, so a
recipe merged here shows up there.

| Recipe | What you get | State |
|---|---|---|
| [Caption screen](recipes/screen-only/recipe.json) | A small screen showing captions. About an hour, no soldering | designed |
| [One-eye caption glasses](recipes/one-eye/recipe.json) | Captions in front of one eye; knows where voices are and how far | designed |
| [Display glasses with a sensor pod](recipes/display-glasses-pod/recipe.json) | Bought display glasses show captions beside each speaker | designed |
| [Ready-made microphone strip](recipes/respeaker-flex/recipe.json) | The pod with less wiring | designed, not yet built |
| [Strip on the table, with a screen](recipes/strip-table/recipe.json) | Table-top captions with speaker numbers and sides; nothing to wear | designed, not yet built |
| [Strip on the table, with display glasses](recipes/strip-table-glasses/recipe.json) | Captions beside each speaker with no soldering | designed, not yet built |
| [One-eye glasses with the ready-made strip](recipes/one-eye-flex/recipe.json) | The homemade one-eye screen, with the bought strip listening | designed, not yet built |
| [Homemade microphone strip](recipes/homemade-strip/recipe.json) | The strip, made from four microphones on a ruler; no soldering | designed, not yet built |
| [One-eye glasses with a microphone strip](recipes/one-eye-strip/recipe.json) | All homemade: the one-eye screen plus the row of microphones | designed, not yet built |

"Designed" means written down and not yet built by anyone. If you build
one, tell us: that is the most useful thing you can add.

## What is here

| Path | What it is |
|---|---|
| `recipes/` | One folder per recipe, and `index.json`, the file the guide reads |
| `parts.json` | Every part any recipe uses: name, what it is, price, and where to buy it |
| `firmware/` | The program for a Seeed XIAO ESP32-S3 (Sense), a PlatformIO project |
| `docs/PROTOCOL.md` | The Bluetooth link between the glasses and HearAura |
| `docs/DESIGN-NOTES.md` | Why the parts were chosen, wiring, and what "3D" honestly means |
| `tests/test_doa.c` | Runs the direction sums on simulated sound |
| `hosts/` | Apps that can drive the glasses, and a small example for a laptop |
| `scripts/check.py` | Checks every recipe and rebuilds `recipes/index.json` |
| `scripts/find_parts.py` | Looks each part up at its shops and writes back today's price and whether it is in stock |

## One program for every recipe

The firmware looks for each part when it starts and uses what it finds:
screen, motion sensor, each pair of microphones, camera. With no captions
to show, the screen reports what it found:

```
HearAura Glasses
phone: connected
motion: yes    eye: yes
ears: 4
voice: left -58
```

Two microphones tell left from right. Four add ahead and behind.

### Putting it on a board

The easy way: lesson 2 of the guide installs it from Chrome or Edge.

Building it yourself, with [PlatformIO](https://platformio.org/):

```
cd firmware
pio run                # build
pio run -t upload      # build and send to the board over USB
pio device monitor     # watch what it prints
```

`platformio.ini` pins the versions it was built against. Newer NimBLE (2.x)
and newer Arduino-ESP32 (3.x) will not build without changes. If an upload
will not start, hold the board's B button while plugging in the USB cable.

### Things to set for your own build

At the top of `firmware/src/main.cpp`: `MIC_WIDTH` and `MIC_DEPTH` (your
microphone spacing, in metres) and `REAR_OFFSET_SAMPLES` (clap from exactly
one side, read the `front/rear delay` figure on the serial monitor, and put
it here so it reads 0). In `firmware/src/eye.cpp`: `CAMERA_HFOV`, if you fit
a wide-angle lens.

## Checking your work

```
python3 scripts/check.py --check                       # recipes and parts
cc -O2 -o test_doa tests/test_doa.c -lm && ./test_doa   # the direction sums
cd firmware && pio run                                  # the firmware builds
```

## What has and has not been tested

| | State |
|---|---|
| Direction and face sums (`firmware/src/doa.h`) | Pass on simulated sound: worst error 0.3 degrees. A real room with echo will be much worse; not measured |
| Firmware | Builds for the XIAO ESP32-S3 (about 1.0 MB). **Never run on a board** |
| Rear microphones sharing the front pair's clock | Written from the chip's documentation; the most likely part to need bench work |
| Any recipe, on a face | Nobody has built one yet |

## Privacy

Camera pictures are analysed on the board and dropped. Only angles and a
distance go to the iPhone. Speakers are numbered by where they are; nobody
is recognised, and nothing about a voice or a face is stored.

## Licence

No licence has been chosen yet. Until one is, you may read and build from
this for yourself, and send changes back here; ask before redistributing.
Questions: support@hearaura.app.
