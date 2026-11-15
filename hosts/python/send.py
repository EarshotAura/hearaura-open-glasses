#!/usr/bin/env python3
"""Sends text to open glasses from any computer with Bluetooth.

    pip install bleak
    echo "Hello" | python3 send.py            each line becomes a finished caption
    some-program | python3 send.py --note     each line is a note (nobody said it)
    python3 send.py --about                   print what the glasses say they are

Every line read from standard input is sent as it arrives, so anything that
prints text can drive the glasses: speech recognition, a translator, a
language model. The format is in ../../docs/PROTOCOL.md.

Not yet run against real glasses.
"""
import asyncio, sys
from bleak import BleakClient, BleakScanner

SERVICE  = "8e7f1a50-0871-4a2c-9d4b-6a1b5c0e0001"
CAPTIONS = "8e7f1a50-0871-4a2c-9d4b-6a1b5c0e0002"
ABOUT    = "8e7f1a50-0871-4a2c-9d4b-6a1b5c0e0005"
FINAL, CLEAR, NOTE = 2, 3, 4
UNKNOWN_BEARING = 0x80

def frames(kind, text, room):
    """Splits text into frames of at most `room` bytes, never inside a character."""
    data, out = text.encode("utf-8"), []
    while True:
        cut = min(len(data), room - 4)
        while 0 < cut < len(data) and (data[cut] & 0xC0) == 0x80:
            cut -= 1
        piece, data = data[:cut], data[cut:]
        out.append(bytes([kind, 0, UNKNOWN_BEARING, 1 if data else 0]) + piece)
        if not data:
            return out

async def main():
    kind = NOTE if "--note" in sys.argv else FINAL
    print("Looking for glasses...", file=sys.stderr)
    device = await BleakScanner.find_device_by_filter(
        lambda d, ad: SERVICE in [u.lower() for u in ad.service_uuids], timeout=20)
    if not device:
        sys.exit("No glasses found. Are they powered, and not already connected to a phone?")
    async with BleakClient(device) as glasses:
        print(f"Connected to {device.name}.", file=sys.stderr)
        about = None
        try:
            about = (await glasses.read_gatt_char(ABOUT)).decode("utf-8", "replace")
        except Exception:
            pass                                 # older glasses have no About
        if "--about" in sys.argv:
            print(about or "These glasses do not say (older firmware).")
            return
        if kind == NOTE and (about is None or "4" not in about.split('"kinds"')[-1].split("]")[0]):
            print("These glasses do not understand notes; sending as captions.", file=sys.stderr)
            kind = FINAL
        room = max(20, glasses.mtu_size - 3)
        loop = asyncio.get_running_loop()
        while True:
            line = await loop.run_in_executor(None, sys.stdin.readline)
            if not line:
                break
            if not line.strip():
                continue
            for frame in frames(kind, line.strip(), room):
                await glasses.write_gatt_char(CAPTIONS, frame, response=False)
        await glasses.write_gatt_char(CAPTIONS, bytes([CLEAR, 0, UNKNOWN_BEARING, 0]), response=False)

if __name__ == "__main__":
    asyncio.run(main())
