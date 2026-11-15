# Open glasses link, version 1

Any Bluetooth LE device that offers this service can be driven by any app
that speaks it. The glasses side is in `../firmware/`. The first app is
HearAura on iPhone; nothing in the link is particular to HearAura, to
iPhone, or to captions from speech. `../hosts/` lists the apps and has a
small example that runs on a laptop.

Two words used below: the **glasses** are the device you wear, and the
**host** is whatever drives them (a phone, a laptop, a small computer in a
pocket).

## Where the thinking happens

The link does not care. Three places are possible, and a build can mix them:

- **On the host.** Turning speech into text, translating, summarising,
  answering a question. The host sends the result as text.
- **On the glasses.** The board finds where a voice is and how far away a
  face is today. Other things a small board can recognise by itself (a
  doorbell, a name being called, a wake word) have a channel reserved below.
- **Somewhere else.** A host may pass work to another machine. That is the
  host's business and its privacy promise to make; the link carries only
  what the tables below say.

## Service

| | UUID | Direction | Properties |
|---|---|---|---|
| Service | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0001` | | advertise this UUID |
| Captions | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0002` | host to glasses | write without response |
| Sensors | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0003` | glasses to host | notify (optional) |
| Audio | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0004` | glasses to host | notify (reserved) |
| About | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0005` | glasses to host | read (optional) |
| Events | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0006` | glasses to host | notify (reserved) |

Only Captions is required. No pairing or bonding is needed.

## Rules that keep old and new working together

1. Bytes that are defined never change meaning.
2. New things get a new kind number or a new characteristic.
3. Glasses ignore a caption kind they do not know. Hosts ignore a
   characteristic, an About field or an event they do not know.
4. A host reads About, if it is there, before sending anything newer than
   kinds 1 to 3.

## Caption frames (host to glasses)

| Byte | Meaning |
|---|---|
| 0 | Kind (see below) |
| 1 | Speaker: 0 = unknown or nobody, otherwise the anonymous speaker's number (1, 2, 3 and so on) |
| 2 | Bearing: signed, in steps of 2 degrees, where the speaker is relative to straight ahead, positive to the wearer's right. -128 (0x80) = unknown |
| 3 | Flags: bit 0 set = more frames of this text follow |
| 4... | UTF-8 text. Text is split only between characters, so each frame is valid UTF-8 on its own |

| Kind | Meaning |
|---|---|
| 1 | Live: the sentence being spoken. Replaces the previous live text |
| 2 | Final: a finished sentence |
| 3 | Clear: captions stopped |
| 4 | Note: text that nobody said. A translation, an answer from an assistant, a summary, an alert. Shown like a final sentence with no speaker |
| 5 to 127 | Reserved for later versions of this link |
| 128 to 255 | Yours: for experiments between your own host and your own glasses |

A long text arrives as several frames; join them until a frame with bit 0
clear. Live text is sent at most five times a second. Ask for a large MTU
(185 or more is typical on iPhone) and never send less than 20 bytes a frame.

A minimal display shows the newest live text, and replaces it with nothing
on clear. A fuller one keeps the last one or two final sentences above it,
draws each speaker in its own colour, and puts the text on the side the
bearing points to.

## Sensor reports (glasses to host, optional)

Eight bytes, about ten times a second. Each value is a little-endian signed
16-bit count of hundredths of a degree.

| Bytes | Meaning |
|---|---|
| 0-1 | Yaw: which way the head points. Grows turning right. Any fixed zero. |
| 2-3 | Pitch |
| 4-5 | Roll |
| 6-7 | Voice bearing: where the loudest voice is relative to straight ahead, positive to the right. 0x7FFF = nobody speaking |

Glasses with a camera may send four more bytes (twelve in all). The camera
picture stays on the glasses; only these numbers are sent.

| Bytes | Meaning |
|---|---|
| 8-9 | Voice elevation: how far above (positive) or below straight ahead the speaker's face is, same units. 0x7FFF = unknown |
| 10-11 | Voice distance in centimetres, unsigned, estimated from the size of the face. 0 = unknown |

What a host does with these is up to it. HearAura adds yaw and voice bearing
to get a direction in the room, and numbers speakers by direction: voices
within 25 degrees of each other are one person, up to six people. Nobody is
identified and nothing about a voice is stored. The speaker number and the
bearing relative to the head's current direction then come back in every
caption frame.

## About (glasses to host, optional)

One read returns a short piece of JSON saying what these glasses are, so a
host can decide what to send:

```json
{"link":1,"firmware":"0.4.0","screen":[128,64],"motion":true,
 "mics":"row","camera":true,"kinds":[1,2,3,4],"ai":["voice-direction","face-distance"]}
```

| Field | Meaning |
|---|---|
| `link` | The version of this document the glasses follow |
| `firmware` | The glasses' own program version, for people to read |
| `screen` | Width and height in pixels of a screen on the glasses, or `null` |
| `motion` | Whether sensor reports carry real head direction |
| `mics` | `none`, `pair`, `row`, `corners` or `strip` |
| `camera` | Whether there is a camera |
| `kinds` | The caption kinds the glasses understand |
| `ai` | What the glasses work out by themselves. Today: `voice-direction`, `face-distance`. New names may appear; ignore ones you do not know |

Glasses without About are treated as understanding kinds 1 to 3.

## Audio (reserved)

Planned: the glasses' microphone as 16 kHz mono mu-law: a 4-byte big-endian
sequence number, then one byte per sample. No host subscribes to it yet.

## Events (reserved)

Planned: things the glasses recognise by themselves, sent when they happen:
one byte for the kind of event, then a short UTF-8 label (for example a
sound the board has learned, or a wake word). Nothing sends or reads it yet.
It is here so that a board that can recognise something has a place to say
so without changing anything above.

A picture channel is deliberately not reserved. Pictures staying on the
glasses is a promise this project makes to the people the camera points at.

## Not tested on hardware

`firmware/src/doa.h` holds the direction maths. `tests/test_doa.c` runs it on
simulated sound: `cc -O2 -o test_doa tests/test_doa.c -lm && ./test_doa`.
The caption and sensor byte formats are pinned by unit tests in the HearAura
app. As of this writing the link has not been exercised on real hardware. If
you build against it, open a build report.
