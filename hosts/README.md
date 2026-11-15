# Hosts

A host is whatever drives the glasses over the link in
[`../docs/PROTOCOL.md`](../docs/PROTOCOL.md). The link is not tied to one
app, one phone or one kind of intelligence.

| Host | Runs on | What it sends | State |
|---|---|---|---|
| [HearAura](https://hearaura.app) | iPhone | Live captions from speech, with speaker numbers and sides | In TestFlight builds after 1.4.1 |
| [`python/send.py`](python/send.py) | A laptop or a small computer (Linux, macOS, Windows) | Any text you pipe into it | Written, not yet run against real glasses |

`python/send.py` is deliberately tiny. It is the starting point for
anything that produces text: speech recognition running on a laptop, a
translator, a local language model answering a question, a script that
reads out notifications.

    pip install bleak
    echo "Hello from a laptop" | python3 hosts/python/send.py
    some-program-that-prints-lines | python3 hosts/python/send.py --note

## Adding a host

Write it anywhere, in anything. Then add a row to the table above with a
link. If the host needs something the link cannot carry yet, open an issue
before inventing bytes: kinds 128 to 255 are yours to experiment with, and
anything meant for everyone gets a number in PROTOCOL.md.
