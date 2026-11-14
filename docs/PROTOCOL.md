# HearAura open glasses link, version 1

Any Bluetooth LE device that offers this service can show HearAura's live
captions. It is meant for glasses you build or reprogram yourself. Turn it on
in HearAura under Settings > Captions in glasses > Look for homemade glasses.
HearAura then scans for the service, connects to the first device it finds,
and reconnects when the link drops.

The byte formats below are pinned by unit tests in
the HearAura app's own tests.

## Service

| | UUID | Direction | Properties |
|---|---|---|---|
| Service | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0001` | | advertise this UUID |
| Captions | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0002` | iPhone to glasses | write without response |
| Sensors | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0003` | glasses to iPhone | notify (optional) |
| Audio | `8E7F1A50-0871-4A2C-9D4B-6A1B5C0E0004` | glasses to iPhone | notify (reserved, not read yet) |

Only Captions is required. No pairing or bonding is needed.

## Caption frames (iPhone to glasses)

| Byte | Meaning |
|---|---|
| 0 | Kind: 1 = live (the sentence being spoken; replaces the previous live text), 2 = final (a finished sentence), 3 = clear (captions stopped) |
| 1 | Speaker: 0 = unknown, otherwise the anonymous speaker's number (1, 2, 3 and so on) |
| 2 | Bearing: signed, in steps of 2 degrees, where the speaker is relative to straight ahead, positive to the wearer's right. -128 (0x80) = unknown |
| 3 | Flags: bit 0 set = more frames of this caption follow |
| 4... | UTF-8 text. A caption is split only between characters, so each frame is valid UTF-8 on its own |

A long caption arrives as several frames; join their text until a frame with
bit 0 clear. Live text is sent at most five times a second. HearAura sizes
frames to the connection (ask for a large MTU; 185 or more is typical on
iPhone) and never sends less than 20 bytes a frame.

A minimal display shows the newest live text, and replaces it with nothing
on clear. A fuller one keeps the last one or two final sentences above it,
draws each speaker in its own colour, and puts the text on the side the
bearing points to.

## Sensor reports (glasses to iPhone, optional)

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

When reports arrive, HearAura adds yaw and voice bearing to get a direction
in the room, and numbers speakers by direction: voices within 25 degrees of
each other are one person, up to six people. Nobody is identified and nothing
about a voice is stored. The speaker number and the bearing relative to the
head's current direction then come back in every caption frame, and a
plugged-in display draws the live caption on that side.

Without sensor reports, speaker is 0 and bearing is unknown.

## Audio (reserved)

Planned: the glasses' microphone as 16 kHz mono mu-law, the same packets the
Apple Watch sends : a 4-byte
big-endian sequence number, then one byte per sample. HearAura does not
subscribe to it yet.

## Not tested on hardware

`firmware/src/doa.h` holds the direction maths (voice bearing from four microphones, and
a face box as bearing, elevation and distance). `test_doa.c` runs it on
simulated sound: `cc -O2 -o test_doa tests/test_doa.c -lm && ./test_doa`.

As of this writing the link has been exercised only by unit tests. If you
build against it, tell us what happened: support@hearaura.app.
